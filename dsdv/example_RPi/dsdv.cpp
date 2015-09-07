#include <RF24/RF24.h>
#include <RF24Network/RF24Network.h>
#include <RF24Network/Rlog.h>

#define MAX_SIZE_of_DVT 10
//超过10s未收到消息，表明连接断开
#define t_alive 10

//************静态结构************
//距离向量条目
struct DV_entry
{
	uint16_t node_dst;
	uint16_t node_nxt;
	uint16_t metric_count;
}DV_table[MAX_SIZE_of_DVT];

//传递距离向量表的消息结构
struct payload_t
{
	uint16_t size;
	struct DV_entry* data;
};

//本地节点的地址
const uint16_t node_address=010;

//邻居节点的地址
uint16_t neighbor[MAX_SIZE_of_DVT];
//邻居节点的数目
uint16_t neighbor_num;

//距离向量表的大小
uint16_t dv_size;

//路由表，每一项为<目的地址，下一跳地址>
std::map<uint16_t,uint16_t> route_table;
std::map<uint16_t,uint16_t>::iterator it;
//保活计数表，每一项为<邻居地址，未收到响应的时间>
std::map<uint16_t,uint16_t> neighbor_alive_count;

FILE * fp;

RF24 radio(RPI_V2_GPIO_P1_15, BCM2835_SPI_CS0, BCM2835_SPI_SPEED_8MHZ);  

RF24Network network(radio);

void dsdv()
{
	//距离向量表初始化
	DV_table[0].node_dst = node_address;
	DV_table[0].node_nxt = node_address;
	DV_table[0].metric_count = 0;
	dv_size = 1;
	route_table[node_address] = node_address;
	//将初始化的路由表写入文件中
	fp=fopen("route.dat","wb");
	for(it=route_table.begin();it!=route_table.end();++it)
		fprintf(fp,"%d,%d\n",it->first,it->second);
	fclose(fp);
	
	uint16_t route_change_flag = 0;
	while(1)
	{
		delay(5);//sending interval is 5s
		
		//send local D-V table periodically
		
		payload_t pl;
		pl.size = dv_size;
		//pl.data = (DV_entry*) malloc(sizeof(DV_entry) * dv_size);
		pl.data = new DV_entry[sizeof(DV_entry) * dv_size];
		for(int i=0; i < dv_size; i++)
			*(pl.data + i) = DV_table[i];
		
		//broadcast local DV table to each node
		for(int i = 0; i < neighbor_num ; i++)
		{		
			//param dst node
			//param header type
			RF24NetworkHeader header(neighbor[i], DSDV_TYPE);
			//这里应该用直接写，即不通过中间节点路由			
			//***修改RF24Network中的判断逻辑，如果类型是dsdv，直接写
			bool ok = network.write(header, &pl, sizeof(payload_t));
		}		
		delete pl.data;
		
		//接收与维护过程
		network.update();	
		while(network.available_dsdv())
		{
			//从本地缓冲队列中提取报文处理
			RF24NetworkHeader header; 
			payload_t msg;
			uint16_t len = sizeof(payload_t);

			network.read_dsdv(header, &msg, len);
			DV_entry* dv_data = msg.data;
			
			//打印收到的距离向量表
			printf("*********DV received from node %d\n", header.from_node);
			printf(" dst \t nxt \t metric\n");
			for(int i = 0; i < msg.size ; i++)
				printf("%d \t%d\t%d\n",(*(dv_data + i)).node_dst,(*(dv_data + i)).node_nxt,(*(dv_data + i)).metric_count);
			printf("********************************\n");
				
			//todo parse the msg to obtain D-V table	
			//todo using D-V table to update route table
			uint16_t flag = 0;
			for(int j = 0 ; j < msg.size ; j++)
			{
				for(int i = 0 ; i < dv_size; i++)
					//已有该目的地址对应的路由项
					if((*(dv_data + j)).node_dst == DV_table[i].node_dst)
					{
						flag =1;
						//修改对应的路由项
						if((*(dv_data + j)).metric_count + 1 < DV_table[i].metric_count){
							DV_table[i].metric_count = (*(dv_data + j)).metric_count + 1;
							DV_table[i].node_nxt = header.from_node;
							route_table[DV_table[i].node_dst] = header.from_node;
							//发生修改标志置1
							route_change_flag = 1;
						}						
					}
				//增加新的路由项
				if(flag == 0)
				{
					DV_table[dv_size].metric_count = (*(dv_data + j)).metric_count + 1;
					DV_table[dv_size].node_nxt = header.from_node;
					DV_table[dv_size].node_dst = (*(dv_data + j)).node_dst;
					route_table[DV_table[dv_size].node_dst] = header.from_node;
					dv_size++;
					
					//发生修改标志置1
					route_change_flag = 1;
				}
				flag = 0;
			}
			
			//收到某节点的信息表示与它的连接还存在，保活计数置 0
			neighbor_alive_count[header.from_node] = 0;
		}//end of dsdv_queue reading
		
		//更新保活计数
		for(int i = 0 ; i < dv_size ; i++)
		{
			//本次更新未收到某个节点的消息，则保活计数加 1
			//本地节点不会给自己发送DV表，所以不需要保活计数项
			if((neighbor_alive_count[DV_table[i].node_nxt] != 0) & (DV_table[i].node_nxt != node_address))
			{
				uint16_t addr_tmp = DV_table[i].node_nxt;
				neighbor_alive_count[addr_tmp]++;
				//如果超过阈值t_alive仍未收到消息，认为连接断开
				//1.去除本地维护的距离向量表中对应项；2.去除以该节点为下一跳的路由项
				//3.删除距离向量表中从本地至该节点的项以及以该节点为中继的项 4.找到合适的下一跳 
				if(neighbor_alive_count[addr_tmp] > t_alive)
				{
					DV_table[i] = DV_table[dv_size - 1];
					dv_size--;
					if(i != 0)
						i--;
					//删除该节点对应的保活计数项
					neighbor_alive_count.erase(addr_tmp);
					
					//修改以该节点为下一跳的路由表项					
					for(it=route_table.begin();it!=route_table.end();++it){
						if(it->second == addr_tmp)
						{
							//route_table.erase(it->first);
							uint16_t new_nxt_find=0;
							for(i = 0 ; i < dv_size ; i++)
								if((DV_table[i].node_nxt != it->second) && (DV_table[i].node_dst == it->first)){
									route_table[it->first] = DV_table[i].node_nxt;
									route_change_flag = 1;
									new_nxt_find=1;
									break;
								}
							if(!new_nxt_find){
								route_table.erase(it->first);
								dv_size--;
								route_change_flag = 1;
							}								
							break;
						}
					}						
				}
			}
		}
		//路由表修改完成
		//如果发生修改，写入本地的route.dat文件
		if( route_change_flag ){
			fp=fopen("route.dat","wb");
			for(it=route_table.begin();it!=route_table.end();++it)
				fprintf(fp,"%d,%d\n",it->first,it->second);
			fclose(fp);
			route_change_flag = 0;
		}
	}
}

int main()
{
	radio.begin();
	
	delay(5);
	network.begin(/*channel*/ 90, /*node address*/ node_address);
	
	dsdv();
	return 0;
}
/*
//****************************************************

std::queue<RF24NetworkFrame> dsdv_queue;

uint16_t RF24Network::read_dsdv(RF24NetworkHeader& header,void* message, uint16_t maxlen)
{
	uint16_t bufsize = 0;

	if ( available_dsdv() ) 
	{
		RF24NetworkFrame frame = dsdv_queue.front();

		// How much buffer size should we actually copy?
		bufsize = std::min(frame.message_size,maxlen);
		memcpy(&header,&(frame.header),sizeof(RF24NetworkHeader));
		memcpy(message,frame.message_buffer,bufsize);

		IF_SERIAL_DEBUG(printf("%u: FRG message size %i\n",millis(),frame.message_size););
		IF_SERIAL_DEBUG(printf("%u: FRG message ",millis()); const char* charPtr = reinterpret_cast<const char*>(message); for (uint16_t i = 0; i < bufsize; i++) { printf("%02X ", charPtr[i]); }; printf("\n\r"));	
	
		IF_SERIAL_DEBUG(printf_P(PSTR("%u: NET read %s\n\r"),millis(),header.toString()));

		dsdv_queue.pop();
	}
	return bufsize;
}

bool RF24Network::available_dsdv(void)
{
	return (!dsdv_queue.empty());
}

//替换RF24Network中的enqueue，当发现是DSDV_TYPE后，加入到dsdv_queue中
uint8_t RF24Network::enqueue(RF24NetworkHeader* header) {
  uint8_t result = false;
  
  RF24NetworkFrame frame = RF24NetworkFrame(*header,frame_buffer+sizeof(RF24NetworkHeader),frame_size-sizeof(RF24NetworkHeader)); 
  
  bool isFragment = ( frame.header.type == NETWORK_FIRST_FRAGMENT || frame.header.type == NETWORK_MORE_FRAGMENTS || frame.header.type == NETWORK_LAST_FRAGMENT || frame.header.type == NETWORK_MORE_FRAGMENTS_NACK);
  
  // This is sent to itself
  if (frame.header.from_node == node_address) {    
    if (isFragment) {
      printf("Cannot enqueue multi-payload frames to self\n");
      result = false;
    }else{
    frame_queue.push(frame);
    result = true;
	}
  }else  
  if (isFragment)
  {
    //The received frame contains the a fragmented payload
    //Set the more fragments flag to indicate a fragmented frame
    IF_SERIAL_DEBUG_FRAGMENTATION_L2(printf("%u: FRG Payload type %d of size %i Bytes with fragmentID '%i' received.\n\r",millis(),frame.header.type,frame.message_size,frame.header.reserved););
    //Append payload
    result = appendFragmentToFrame(frame);
   
    //The header.reserved contains the actual header.type on the last fragment 
    if ( result && frame.header.type == NETWORK_LAST_FRAGMENT) {
	  IF_SERIAL_DEBUG_FRAGMENTATION(printf("%u: FRG Last fragment received. \n",millis() ););
      IF_SERIAL_DEBUG(printf_P(PSTR("%u: NET Enqueue assembled frame @%x "),millis(),frame_queue.size()));

	  RF24NetworkFrame *f = &(frameFragmentsCache[ frame.header.from_node ] );
	  
	  result=f->header.type == EXTERNAL_DATA_TYPE ? 2 : 1;
	  
	  //Load external payloads into a separate queue on linux
	  if(result == 2){
	    external_queue.push( frameFragmentsCache[ frame.header.from_node ] );
	  //tq 修改
	  }else if(frame.header.type == DSDV_TYPE){
		  dsdv_queue.push( frameFragmentsCache[ frame.header.from_node ] );
	  }else{
        frame_queue.push( frameFragmentsCache[ frame.header.from_node ] );
	  }
      frameFragmentsCache.erase( frame.header.from_node );
	}

  }else{//  if (frame.header.type <= MAX_USER_DEFINED_HEADER_TYPE) {
    //This is not a fragmented payload but a whole frame.

    IF_SERIAL_DEBUG(printf_P(PSTR("%u: NET Enqueue @%x "),millis(),frame_queue.size()));
    // Copy the current frame into the frame queue
	result=frame.header.type == EXTERNAL_DATA_TYPE ? 2 : 1;
    //Load external payloads into a separate queue on linux
	//ALso load dsdv msg into a separate queue
	if(result == 2){
	  external_queue.push( frame );
	//tq 修改
	}else if(frame.header.type == DSDV_TYPE){
      dsdv_queue.push( frame );
	}else
		frame_queue.push( frame );
	

  }
  if (result) {
    //IF_SERIAL_DEBUG(printf("ok\n\r"));
  } else {
    IF_SERIAL_DEBUG(printf("failed\n\r"));
  }

  return result;
}

//输入目的地址，通过路由表转化出下一跳和对应的pipe号
bool RF24Network::logicalToPhysicalAddress(logicalToPhysicalStruct *conversionInfo){
  //Create pointers so this makes sense.. kind of
  //We take in the to_node(logical) now, at the end of the function, output the send_node(physical) address, etc.
  //back to the original memory address that held the logical information.
  uint16_t *to_node = &conversionInfo->send_node;
  uint8_t *directTo = &conversionInfo->send_pipe;
  bool *multicast = &conversionInfo->multicast;    
  
	uint16_t arg1,arg2;
	fp=fopen("route.dat","rb");
	while(!feof(fp)){
		fscanf(fp,"%d,%d\n",&arg1,&arg2);
		route_table[arg1]=arg2;
		log(INFO,"route entry:(target)%#o->(nxt node)%#o\n",arg1,arg2);
	}
	fclose(fp);	
  
	if(*directTo > TX_ROUTED ){    
		pre_conversion_send_node = *to_node;
		*multicast = 1;
		if(*directTo == USER_TX_MULTICAST){
			pre_conversion_send_pipe=0;
		}	
	}     

  //判断为dsdv消息时，直接转发
  RF24NetworkHeader* header = (RF24NetworkHeader*)&frame_buffer;
  if(header->type == DSDV_TYPE)
	  *directTo = 5;
  else 
	  if(route_table.find(*to_node) != 0)
	  {
		  *to_node = route_table[*to_node];
		  *directTo = 5;
	  }
	  else
		//目的不可达
		printf("***cannot send or relay this msg, no routing entry!");  
  return 1; 
}


//增加DSDV_TYPE
#define DSDV_TYPE 20

*/