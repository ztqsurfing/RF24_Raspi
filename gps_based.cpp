#include <RF24/RF24.h>
#include <RF24Network/RF24Network.h>
#include <log.h>

#include <math.h>

#define EARTH_RADIUS 6378.137
#define PI 3.1415926535898
#define MAX_NUM_of_NODE 10
#define d_threshold 40

//交互gps的消息格式，本地的gps也适用该结构进行存储
struct payload_t
{
	double lat;
	double lon;
};

struct GPS
{
	double lat;
	double lon;
}localgps;

const uint16_t node_address=010;
uint16_t node_id;

//邻居节点的地址
uint16_t neighbor[MAX_NUM_of_NODE];
//邻居节点的数目
uint16_t neighbor_num;

//存储节点地址到id的映射 <节点地址，节点id>
std::map<uint16_t,uint16_t> addr_to_id;
uint16_t id_inc = 0;

//GPS表，记录每个节点的GPS
struct GPS GPS_table[MAX_NUM_of_NODE];
//节点距离表，记录节点之间的距离
uint16_t nodeDis_table[MAX_NUM_of_NODE][MAX_NUM_of_NODE];

//路由表，每一项为<目的地址，下一跳地址>
std::map<uint16_t,uint16_t> route_table;

//record the break link due to distance limit
uint16_t monitor[MAX_NUM_of_NODE];
uint16_t monitor_count=0;

double rad(double d)
{
	return d * PI / 180.0;
}

//google地图上测试比较准确
double GetDistance(double lat1, double lng1, double lat2, double lng2)
{
	double radLat1 = rad(lat1);
	double radLat2 = rad(lat2);
	double a = radLat1 - radLat2;
	double b = rad(lng1) - rad(lng2);
	double s = 2 * asin(sqrt(pow(sin(a/2),2) + cos(radLat1)*cos(radLat2)*pow(sin(b/2),2)));
	s = s * EARTH_RADIUS;
	s = (round)(s * 10000000) / 10000;
	return s;
}

void updateGPS()
{
	
}

uint16_t id_to_addr(uint16_t id)
{
	std::map<uint16_t,uint16_t>::iterator it;
	for(it=route_table.begin();it!=route_table.end();++it){
		if(it->second == id)
			return it->first;
	}
	return 0;
}

void position_route()
{
	//初始化节点距离表
	for(int i=0;i<MAX_NUM_of_NODE;i++)
	{
		for(int j=0;j<MAX_NUM_of_NODE;j++)
		{
			if(i == j)
				nodeDis_table[i][j]=0;
			else
				nodeDis_table[i][j]=10000;
		}
	}
	//将0作为本地的id
	node_id = addr_to_id[node_address] = id_inc++;
	//初始化路由表
	//全部一跳发送
	for(int i=0;i<MAX_NUM_of_NODE;i++)
		route_table[neighbor[i]] = neighbor[i];
	//将初始化的路由表写入文件中
	fp=fopen("route.dat","wb");
	for(it=route_table.begin();it!=route_table.end();++it)
		fprintf(fp,"%d,%d\n",it->first,it->second);
	fclose(fp);
	uint16_t route_change_flag = 0;
	
	while(1)
	{
		sleep(1);//sending interval is 1s
		
		updateGPS();
		payload_t pl = {localgps.lat, localgps.lon};
		//将本地的GPS信息发送给所有节点
		for(i = 0; i < neighbor_num ; i++)
		{		
			//param dst node
			//param header type
			RF24NetworkHeader header(neighbor[i], GPS_TYPE);
			//这里应该用直接写，即不通过中间节点路由			
			//***修改RF24Network中的判断逻辑，如果类型是dsdv，直接写
			bool ok = network.write(header, &pl, sizeof(payload_t));
		}
		
		//接收与维护过程
		network.update();	
		while(network.available_gps())
		{
			//从本地缓冲队列中提取报文处理
			//RF24NetworkHeader header; 
			payload_t msg;
			uint16_t len = sizeof(payload_t);

			network.read_gps(&header, &msg, len);
				
			//打印收到的GPS信息
			log(INFO,"*********GPS received from node %d\n", header.from_node);
			log(INFO," latitude:%f, longitude:%f\n",msg.lat,msg.lon);
			
			//建立ip-id映射关系
			if( addr_to_id.find(header.from_node) == 0 )
				addr_to_id[header.from_node] = id_inc++;
			
			//维护GPS表、距离表
			uint16_t y = addr_to_id[header.from_node];
			GPS_table[y].lat = msg.lat;
			GPS_table[y].lon = msg.lon;		
			for(i=0;i<id_inc;i++)
			{
				if(i != y){
					uint16_t d_tmp = GetDistance(GPS_table[i].lat,GPS_table[i].lon,GPS_table[y].lat,GPS_table[y].lon);
					nodeDis_table[i][y] = d_tmp;
					nodeDis_table[y][i] = d_tmp;						
				}
			}
			//距离恢复到一跳可达，则直接连接
			for(i=0;i<monitor_count;i++)
			{
				uint16_t addr = monitor[i];
				uint16_t id_tmp = addr_to_id[addr];
				if(nodeDis_table[node_id][id_tmp] < d_threshold)
				{
					route_table[addr] = addr;
					monitor[i]=monitor[monitor_count-1];
					monitor_count--;
					if(i!=0)
						i--;
				}
			}
			//距离大于一跳可达，则切换路由
			if(nodeDis_table[node_id][y] > d_threshold)
			{
				i=node_id+1;
				uint16_t flag = 0;
				while(i!=y & i<id_inc)
				{
					if((nodeDis_table[node_id][i] < d_threshold) && ( nodeDis_table[i][y] < d_threshold))
					{
						if(id_to_addr(i)! = 0)
							route_table[header.from_node] = id_to_addr(i);
						else{
							log(ERROR,"sth wrong, check your code!");
						}
						flag = 1;
						route_change_flag = 1；
						break;
					}				
				}
				if(flag == 0)
					log(INFO,"node %d unreachable!",id_to_addr(y));
				
				monitor[monitor_count]=y;
				monitor_count++;
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
}

int main()
{
	position_route();
	return 0;
}

//*****************************************

/*
std::queue<RF24NetworkFrame> gps_queue;

uint16_t RF24Network::read_gps(RF24NetworkHeader& header,void* message, uint16_t maxlen)
{
	uint16_t bufsize = 0;

	if ( available_gps() ) 
	{
		RF24NetworkFrame frame = gps_queue.front();

		// How much buffer size should we actually copy?
		bufsize = std::min(frame.message_size,maxlen);
		memcpy(&header,&(frame.header),sizeof(RF24NetworkHeader));
		memcpy(message,frame.message_buffer,bufsize);

		IF_SERIAL_DEBUG(printf("%u: FRG message size %i\n",millis(),frame.message_size););
		IF_SERIAL_DEBUG(printf("%u: FRG message ",millis()); const char* charPtr = reinterpret_cast<const char*>(message); for (uint16_t i = 0; i < bufsize; i++) { printf("%02X ", charPtr[i]); }; printf("\n\r"));	
	
		IF_SERIAL_DEBUG(printf_P(PSTR("%u: NET read %s\n\r"),millis(),header.toString()));

		gps_queue.pop();
	}
	return bufsize;
}

bool RF24Network::available_gps(void)
{
	return (!gps_queue.empty());
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
	  }else if(frame.header.type == GPS_TYPE){
		  gps_queue.push( frameFragmentsCache[ frame.header.from_node ] );
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
	}else if(frame.header.type == GPS_TYPE){
      gps_queue.push( frame );
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
  if(header->type == GPS_TYPE)
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

#define GPS_TYPE 21
*/