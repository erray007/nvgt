/* map_system.cpp - LINUX VE WINDOWS UYUMLU FIXED */
#ifdef _WIN32
#include <windows.h>
#endif
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <cmath>
#include <algorithm>
#include <queue>
#include <sstream>
#include <memory>
#include <new>
#include "../../src/nvgt_plugin.h"
using namespace std;
const int FRAME_SIZES[]={8192,256,32};
const int TOTAL_FRAMES=3;
const int WALL_COST=10;
const int MAX_COORD_LIMIT=1000000;
const int MIN_COORD_LIMIT=-1000000;
const long long HASH_OFFSET=1000000;
struct MapVector3{
float x,y,z;
};
void ConstructVector3(MapVector3* thisPointer){
new(thisPointer)MapVector3();
thisPointer->x=0;thisPointer->y=0;thisPointer->z=0;
}
void ConstructVector3Init(float x,float y,float z,MapVector3* thisPointer){
new(thisPointer)MapVector3();
thisPointer->x=x;thisPointer->y=y;thisPointer->z=z;
}
inline unsigned long long get_coord_hash(int x,int y,int z){
unsigned long long ux=(long long)x+HASH_OFFSET;
unsigned long long uy=(long long)y+HASH_OFFSET;
unsigned long long uz=(long long)z+HASH_OFFSET;
return(ux<<42)|(uy<<21)|uz;
}
inline unsigned long long get_chunk_hash(float x,float y,float z,int frame_size){
if(frame_size<=0)frame_size=32;
int cx=floor(x/frame_size);
int cy=floor(y/frame_size);
int cz=floor(z/frame_size);
unsigned long long ux=(long long)cx+HASH_OFFSET;
unsigned long long uy=(long long)cy+HASH_OFFSET;
unsigned long long uz=(long long)cz+HASH_OFFSET;
return(ux<<42)|(uy<<21)|uz;
}
int get_appropriate_frame_index(float min_x,float max_x,float min_y,float max_y,float min_z,float max_z){
float dx=max_x-min_x;
float dy=max_y-min_y;
float dz=max_z-min_z;
for(int i=0;i<TOTAL_FRAMES;i++){
if(dx>=FRAME_SIZES[i]||dy>=FRAME_SIZES[i]||dz>=FRAME_SIZES[i])return i;
}
return TOTAL_FRAMES-1;
}
struct MapObject{
int id;
string type;
float min_x,max_x,min_y,max_y,min_z,max_z;
bool trackable;
bool stair;
bool is_wall;
int frame_index;
bool contains(float x,float y,float z)const{
return(x>=min_x&&x<=max_x&&y>=min_y&&y<=max_y&&z>=min_z&&z<=max_z);
}
};
struct MapZone{
int id;
string text;
float min_x,max_x,min_y,max_y,min_z,max_z;
bool trackable;
};
struct MapSafeZone{
int id;
float min_x,max_x,min_y,max_y,min_z,max_z;
};
struct MapRestrictedZone{
int id;
float min_x,max_x,min_y,max_y,min_z,max_z;
vector<string>banned_items;
};
struct MapSurvivalZone{
int id;
float min_x,max_x,min_y,max_y,min_z,max_z;
int interval;
int min_val;
int max_val;
};
struct MapReplenishZone{
int id;
float min_x,max_x,min_y,max_y,min_z,max_z;
};
struct MapClimate{
int id;
float min_x,max_x,min_y,max_y,min_z,max_z;
float min_temp;
float max_temp;
bool is_interior;
};
struct MapFX{
int id;
string fx_type;
string params;
float min_x,max_x,min_y,max_y,min_z,max_z;
};
struct Node{
int x,y,z;
float g_cost,h_cost;
Node* parent;
float f_cost()const{return g_cost+h_cost;}
};
struct CompareNode{
bool operator()(Node* a,Node* b){return a->f_cost()>b->f_cost();}
};
class GameMapData{
public:
string name;
string owner;
string description;
string storage_data;
int open_hour;
int close_hour;
float center_x,center_y,center_z;
float world_min_x,world_min_y,world_min_z;
float world_max_x,world_max_y,world_max_z;
unordered_map<unsigned long long,vector<int>>chunks[TOTAL_FRAMES];
map<int,vector<MapObject>>objects;
map<int,vector<MapZone>>zones;
map<int,MapSafeZone>safe_zones;
map<int,MapRestrictedZone>restricted_zones;
map<int,vector<MapSurvivalZone>>thirst_zones;
map<int,vector<MapSurvivalZone>>hunger_zones;
map<int,vector<MapSurvivalZone>>health_zones;
map<int,vector<MapReplenishZone>>thirst_reset_zones;
map<int,vector<MapReplenishZone>>hunger_reset_zones;
map<int,MapFX>effects;
map<int,MapClimate>climates;
GameMapData(string n):name(n),center_x(0),center_y(0),center_z(0),owner(""),description(""),storage_data(""),
open_hour(0),close_hour(24),
world_min_x(0),world_min_y(0),world_min_z(0),
world_max_x(0),world_max_y(0),world_max_z(0){}
};
map<string,unique_ptr<GameMapData>>all_maps;
void register_object_to_chunks(GameMapData* m,MapObject& obj){
if(obj.min_x<MIN_COORD_LIMIT||obj.max_x>MAX_COORD_LIMIT)return;
int f_idx=get_appropriate_frame_index(obj.min_x,obj.max_x,obj.min_y,obj.max_y,obj.min_z,obj.max_z);
obj.frame_index=f_idx;
int size=FRAME_SIZES[f_idx];
if(size<=0)size=32;
int start_cx=floor(obj.min_x/size);
int end_cx=floor(obj.max_x/size);
int start_cy=floor(obj.min_y/size);
int end_cy=floor(obj.max_y/size);
int start_cz=floor(obj.min_z/size);
int end_cz=floor(obj.max_z/size);
long long loop_count=0;
long long max_loops=100000;
for(int x=start_cx;x<=end_cx;x++){
for(int y=start_cy;y<=end_cy;y++){
for(int z=start_cz;z<=end_cz;z++){
loop_count++;
if(loop_count>max_loops)return;
int cx=x;int cy=y;int cz=z;
unsigned long long ux=(long long)cx+HASH_OFFSET;
unsigned long long uy=(long long)cy+HASH_OFFSET;
unsigned long long uz=(long long)cz+HASH_OFFSET;
unsigned long long key=(ux<<42)|(uy<<21)|uz;
m->chunks[f_idx][key].push_back(obj.id);
}
}
}
}
void unregister_object_from_chunks(GameMapData* m,const MapObject& obj){
if(obj.min_x<MIN_COORD_LIMIT||obj.max_x>MAX_COORD_LIMIT)return;
int f_idx=obj.frame_index;
if(f_idx<0||f_idx>=TOTAL_FRAMES)return;
int size=FRAME_SIZES[f_idx];
if(size<=0)size=32;
int start_cx=floor(obj.min_x/size);
int end_cx=floor(obj.max_x/size);
int start_cy=floor(obj.min_y/size);
int end_cy=floor(obj.max_y/size);
int start_cz=floor(obj.min_z/size);
int end_cz=floor(obj.max_z/size);
for(int x=start_cx;x<=end_cx;x++){
for(int y=start_cy;y<=end_cy;y++){
for(int z=start_cz;z<=end_cz;z++){
int cx=x;int cy=y;int cz=z;
unsigned long long ux=(long long)cx+HASH_OFFSET;
unsigned long long uy=(long long)cy+HASH_OFFSET;
unsigned long long uz=(long long)cz+HASH_OFFSET;
unsigned long long key=(ux<<42)|(uy<<21)|uz;
if(m->chunks[f_idx].find(key)!=m->chunks[f_idx].end()){
auto& vec=m->chunks[f_idx][key];
vec.erase(remove(vec.begin(),vec.end(),obj.id),vec.end());
if(vec.empty())m->chunks[f_idx].erase(key);
}
}
}
}
}
map<string,int>parse_rules(string rules){
map<string,int>costs;
stringstream ss(rules);
string segment;
while(getline(ss,segment,';')){
stringstream ss2(segment);
string type,cost_str;
if(getline(ss2,type,':')&&getline(ss2,cost_str)){
costs[type]=stoi(cost_str);
}
}
return costs;
}
float get_dist(float x1,float y1,float z1,float x2,float y2,float z2){
return sqrt(pow(x2-x1,2)+pow(y2-y1,2)+pow(z2-z1,2));
}
void map_create(string name){
if(all_maps.find(name)==all_maps.end()){
all_maps[name]=make_unique<GameMapData>(name);
}
}
bool map_delete(string name){
if(all_maps.find(name)!=all_maps.end()){
all_maps.erase(name);
return true;
}
return false;
}
void map_reset_system(){
all_maps.clear();
}
void map_set_center(string mapName,float x,float y,float z){
if(all_maps.find(mapName)!=all_maps.end()){
all_maps[mapName]->center_x=x;
all_maps[mapName]->center_y=y;
all_maps[mapName]->center_z=z;
}
}
float map_get_center_x(string mapName){
if(all_maps.find(mapName)!=all_maps.end()){
return all_maps[mapName]->center_x;
}
return 0.0f;
}
float map_get_center_y(string mapName){
if(all_maps.find(mapName)!=all_maps.end()){
return all_maps[mapName]->center_y;
}
return 0.0f;
}
float map_get_center_z(string mapName){
if(all_maps.find(mapName)!=all_maps.end()){
return all_maps[mapName]->center_z;
}
return 0.0f;
}
void map_set_owner(string mapName,string owner){
if(all_maps.find(mapName)!=all_maps.end()){
all_maps[mapName]->owner=owner;
}
}
string map_get_owner(string mapName){
if(all_maps.find(mapName)!=all_maps.end()){
return all_maps[mapName]->owner;
}
return "";
}
void map_set_description(string mapName,string desc){
if(all_maps.find(mapName)!=all_maps.end()){
all_maps[mapName]->description=desc;
}
}
string map_get_description(string mapName){
if(all_maps.find(mapName)!=all_maps.end()){
return all_maps[mapName]->description;
}
return "";
}
void map_set_storage(string mapName,string data){
if(all_maps.find(mapName)!=all_maps.end()){
all_maps[mapName]->storage_data=data;
}
}
string map_get_storage(string mapName){
if(all_maps.find(mapName)!=all_maps.end()){
return all_maps[mapName]->storage_data;
}
return "";
}
void map_set_hours(string mapName,int open,int close){
if(all_maps.find(mapName)!=all_maps.end()){
all_maps[mapName]->open_hour=open;
all_maps[mapName]->close_hour=close;
}
}
int map_get_open_hour(string mapName){
if(all_maps.find(mapName)!=all_maps.end()){
return all_maps[mapName]->open_hour;
}
return 0;
}
int map_get_close_hour(string mapName){
if(all_maps.find(mapName)!=all_maps.end()){
return all_maps[mapName]->close_hour;
}
return 24;
}
MapVector3 map_get_min_value(string mapName){
MapVector3 v={0,0,0};
if(all_maps.find(mapName)!=all_maps.end()){
GameMapData* m=all_maps[mapName].get();
v.x=m->world_min_x;
v.y=m->world_min_y;
v.z=m->world_min_z;
}
return v;
}
MapVector3 map_get_max_value(string mapName){
MapVector3 v={0,0,0};
if(all_maps.find(mapName)!=all_maps.end()){
GameMapData* m=all_maps[mapName].get();
v.x=m->world_max_x;
v.y=m->world_max_y;
v.z=m->world_max_z;
}
return v;
}
string map_get_all_map_names(string delimiter){
string result="";
bool first=true;
for(auto const&[name,data]:all_maps){
if(!first)result+=delimiter;
result+=name;
first=false;
}
return result;
}
void map_add_tile(string mapName,float minx,float maxx,float miny,float maxy,float minz,float maxz,string type,int id,bool trackable,bool stair){
if(all_maps.find(mapName)==all_maps.end())return;
GameMapData* m=all_maps[mapName].get();
MapObject obj;
obj.id=id;
obj.type=type;
obj.min_x=minx;obj.max_x=maxx;
obj.min_y=miny;obj.max_y=maxy;
obj.min_z=minz;obj.max_z=maxz;
obj.trackable=trackable;
obj.stair=stair;
obj.is_wall=false;
m->objects[id].push_back(obj);
register_object_to_chunks(m,obj);
}
void map_add_wall(string mapName,float minx,float maxx,float miny,float maxy,float minz,float maxz,string type,int id){
if(all_maps.find(mapName)==all_maps.end())return;
GameMapData* m=all_maps[mapName].get();
MapObject obj;
obj.id=id;
obj.type=type;
obj.min_x=minx;obj.max_x=maxx;
obj.min_y=miny;obj.max_y=maxy;
obj.min_z=minz;obj.max_z=maxz;
obj.trackable=false;
obj.stair=false;
obj.is_wall=true;
m->objects[id].push_back(obj);
register_object_to_chunks(m,obj);
}
void map_remove_tile_from_id(string mapName,int id){
if(all_maps.find(mapName)==all_maps.end())return;
GameMapData* m=all_maps[mapName].get();
if(m->objects.find(id)==m->objects.end())return;
vector<MapObject>& targets=m->objects[id];
for(const auto& obj:targets){
unregister_object_from_chunks(m,obj);
}
m->objects.erase(id);
}
string map_get_tile_at(string mapName,float x,float y,float z){
if(all_maps.find(mapName)==all_maps.end())return"";
GameMapData* m=all_maps[mapName].get();
if(x<MIN_COORD_LIMIT||x>MAX_COORD_LIMIT)return"";
for(int i=TOTAL_FRAMES-1;i>=0;i--){
int size=FRAME_SIZES[i];
unsigned long long chunkKey=get_chunk_hash(x,y,z,size);
if(m->chunks[i].find(chunkKey)!=m->chunks[i].end()){
vector<int>& ids=m->chunks[i][chunkKey];
for(int k=ids.size()-1;k>=0;k--){
int id=ids[k];
if(m->objects.count(id)){
vector<MapObject>& list=m->objects[id];
for(int j=list.size()-1;j>=0;j--){
const auto& obj=list[j];
if(obj.contains(x,y,z)){
return obj.type;
}
}
}
}
}
}
return"";
}
string map_get_tile_index_from_id(string mapName,int id){
if(all_maps.find(mapName)!=all_maps.end()){
GameMapData* m=all_maps[mapName].get();
if(m->objects.count(id)){
vector<MapObject>& list=m->objects[id];
if(list.empty())return"";
float min_x=list[0].min_x,max_x=list[0].max_x;
float min_y=list[0].min_y,max_y=list[0].max_y;
float min_z=list[0].min_z,max_z=list[0].max_z;
for(size_t i=1;i<list.size();i++){
if(list[i].min_x<min_x)min_x=list[i].min_x;
if(list[i].max_x>max_x)max_x=list[i].max_x;
if(list[i].min_y<min_y)min_y=list[i].min_y;
if(list[i].max_y>max_y)max_y=list[i].max_y;
if(list[i].min_z<min_z)min_z=list[i].min_z;
if(list[i].max_z>max_z)max_z=list[i].max_z;
}
return to_string(min_x)+":"+to_string(max_x)+":"+
to_string(min_y)+":"+to_string(max_y)+":"+
to_string(min_z)+":"+to_string(max_z);
}
}
return"";
}
string map_get_tile_list(string mapName,float px,float py,float pz,int distance,string parse_char){
if(all_maps.find(mapName)==all_maps.end())return"";
GameMapData* m=all_maps[mapName].get();
string result="";
bool first=true;
for(auto const&[id,objList]:m->objects){
for(const auto& obj:objList){
float dist=get_dist(px,py,pz,obj.min_x,obj.min_y,obj.min_z);
if(dist<=distance){
if(!first)result+="|";
result+=obj.type+parse_char+
to_string((int)obj.min_x)+parse_char+to_string((int)obj.max_x)+parse_char+
to_string((int)obj.min_y)+parse_char+to_string((int)obj.max_y)+parse_char+
to_string((int)obj.min_z)+parse_char+to_string((int)obj.max_z)+parse_char+
to_string(obj.id);
first=false;
}
}
}
return result;
}
bool map_is_stair(string mapName,float x,float y,float z){
if(all_maps.find(mapName)==all_maps.end())return false;
GameMapData* m=all_maps[mapName].get();
if(x<MIN_COORD_LIMIT||x>MAX_COORD_LIMIT)return false;
for(int i=TOTAL_FRAMES-1;i>=0;i--){
int size=FRAME_SIZES[i];
unsigned long long chunkKey=get_chunk_hash(x,y,z,size);
if(m->chunks[i].find(chunkKey)!=m->chunks[i].end()){
vector<int>& ids=m->chunks[i][chunkKey];
for(int id:ids){
if(m->objects.count(id)){
for(const auto& obj:m->objects[id]){
if(obj.contains(x,y,z)&&obj.stair)return true;
}
}
}
}
}
return false;
}
void map_add_zone(string mapName,float minx,float maxx,float miny,float maxy,float minz,float maxz,int id,string text,bool trackable){
if(all_maps.find(mapName)!=all_maps.end()){
MapZone z={id,text,minx,maxx,miny,maxy,minz,maxz,trackable};
all_maps[mapName]->zones[id].push_back(z);
}
}
void map_remove_zone(string mapName,int id){
if(all_maps.find(mapName)!=all_maps.end()){
all_maps[mapName]->zones.erase(id);
}
}
string map_get_zone_at(string mapName,float x,float y,float z){
if(all_maps.find(mapName)==all_maps.end())return"";
GameMapData* m=all_maps[mapName].get();
for(auto it=m->zones.rbegin();it!=m->zones.rend();++it){
const auto& zoneList=it->second;
for(int i=zoneList.size()-1;i>=0;i--){
const auto& z_obj=zoneList[i];
if(x>=z_obj.min_x&&x<=z_obj.max_x&&
y>=z_obj.min_y&&y<=z_obj.max_y&&
z>=z_obj.min_z&&z<=z_obj.max_z){
return z_obj.text;
}
}
}
return"";
}
string map_get_zone_list(string mapName){
if(all_maps.find(mapName)==all_maps.end())return"";
GameMapData* m=all_maps[mapName].get();
string result="";
bool first=true;
for(auto const&[id,zoneList]:m->zones){
for(const auto& z:zoneList){
if(!first)result+="|";
result+=z.text+":"+
to_string((int)z.min_x)+":"+to_string((int)z.max_x)+":"+
to_string((int)z.min_y)+":"+to_string((int)z.max_y)+":"+
to_string((int)z.min_z)+":"+to_string((int)z.max_z)+":"+
to_string(z.id);
first=false;
}
}
return result;
}
void map_add_safe_zone(string mapName,int id,float minx,float maxx,float miny,float maxy,float minz,float maxz){
if(all_maps.find(mapName)==all_maps.end())return;
MapSafeZone sz;
sz.id=id;
sz.min_x=minx;sz.max_x=maxx;
sz.min_y=miny;sz.max_y=maxy;
sz.min_z=minz;sz.max_z=maxz;
all_maps[mapName]->safe_zones[id]=sz;
}
void map_remove_safe_zone(string mapName,int id){
if(all_maps.find(mapName)!=all_maps.end()){
all_maps[mapName]->safe_zones.erase(id);
}
}
bool map_is_safe(string mapName,float x,float y,float z){
if(all_maps.find(mapName)==all_maps.end())return false;
GameMapData* m=all_maps[mapName].get();
for(auto it=m->safe_zones.rbegin();it!=m->safe_zones.rend();++it){
const auto& sz=it->second;
if(x>=sz.min_x&&x<=sz.max_x&&
y>=sz.min_y&&y<=sz.max_y&&
z>=sz.min_z&&z<=sz.max_z){
return true;
}
}
return false;
}
void map_add_restricted_zone(string mapName,int id,float minx,float maxx,float miny,float maxy,float minz,float maxz,string items){
if(all_maps.find(mapName)==all_maps.end())return;
MapRestrictedZone rz;
rz.id=id;
rz.min_x=minx;rz.max_x=maxx;
rz.min_y=miny;rz.max_y=maxy;
rz.min_z=minz;rz.max_z=maxz;
stringstream ss(items);
string segment;
while(getline(ss,segment,'|')){
if(!segment.empty())rz.banned_items.push_back(segment);
}
all_maps[mapName]->restricted_zones[id]=rz;
}
void map_remove_restricted_zone(string mapName,int id){
if(all_maps.find(mapName)!=all_maps.end()){
all_maps[mapName]->restricted_zones.erase(id);
}
}
bool map_can_use_item(string mapName,float x,float y,float z,string item){
if(all_maps.find(mapName)==all_maps.end())return true;
GameMapData* m=all_maps[mapName].get();
for(auto it=m->restricted_zones.rbegin();it!=m->restricted_zones.rend();++it){
const auto& rz=it->second;
if(x>=rz.min_x&&x<=rz.max_x&&
y>=rz.min_y&&y<=rz.max_y&&
z>=rz.min_z&&z<=rz.max_z){
for(const string& banned:rz.banned_items){
if(banned==item)return false;
}
}
}
return true;
}
void map_add_thirst_zone(string mapName,int id,float minx,float maxx,float miny,float maxy,float minz,float maxz,int interval,int min_val,int max_val){
if(all_maps.find(mapName)==all_maps.end())return;
MapSurvivalZone z={id,minx,maxx,miny,maxy,minz,maxz,interval,min_val,max_val};
all_maps[mapName]->thirst_zones[id].push_back(z);
}
void map_remove_thirst_zone(string mapName,int id){
if(all_maps.find(mapName)!=all_maps.end()){
all_maps[mapName]->thirst_zones.erase(id);
}
}
string map_get_thirst_settings_at(string mapName,float x,float y,float z){
if(all_maps.find(mapName)==all_maps.end())return"";
GameMapData* m=all_maps[mapName].get();
for(auto it=m->thirst_zones.rbegin();it!=m->thirst_zones.rend();++it){
const auto& zoneList=it->second;
for(int i=zoneList.size()-1;i>=0;i--){
const auto& z_obj=zoneList[i];
if(x>=z_obj.min_x&&x<=z_obj.max_x&&y>=z_obj.min_y&&y<=z_obj.max_y&&z>=z_obj.min_z&&z<=z_obj.max_z){
return to_string(z_obj.interval)+":"+to_string(z_obj.min_val)+":"+to_string(z_obj.max_val);
}
}
}
return"";
}
void map_add_hunger_zone(string mapName,int id,float minx,float maxx,float miny,float maxy,float minz,float maxz,int interval,int min_val,int max_val){
if(all_maps.find(mapName)==all_maps.end())return;
MapSurvivalZone z={id,minx,maxx,miny,maxy,minz,maxz,interval,min_val,max_val};
all_maps[mapName]->hunger_zones[id].push_back(z);
}
void map_remove_hunger_zone(string mapName,int id){
if(all_maps.find(mapName)!=all_maps.end()){
all_maps[mapName]->hunger_zones.erase(id);
}
}
string map_get_hunger_settings_at(string mapName,float x,float y,float z){
if(all_maps.find(mapName)==all_maps.end())return"";
GameMapData* m=all_maps[mapName].get();
for(auto it=m->hunger_zones.rbegin();it!=m->hunger_zones.rend();++it){
const auto& zoneList=it->second;
for(int i=zoneList.size()-1;i>=0;i--){
const auto& z_obj=zoneList[i];
if(x>=z_obj.min_x&&x<=z_obj.max_x&&y>=z_obj.min_y&&y<=z_obj.max_y&&z>=z_obj.min_z&&z<=z_obj.max_z){
return to_string(z_obj.interval)+":"+to_string(z_obj.min_val)+":"+to_string(z_obj.max_val);
}
}
}
return"";
}
void map_add_health_zone(string mapName,int id,float minx,float maxx,float miny,float maxy,float minz,float maxz,int interval,int min_val,int max_val){
if(all_maps.find(mapName)==all_maps.end())return;
MapSurvivalZone z={id,minx,maxx,miny,maxy,minz,maxz,interval,min_val,max_val};
all_maps[mapName]->health_zones[id].push_back(z);
}
void map_remove_health_zone(string mapName,int id){
if(all_maps.find(mapName)!=all_maps.end()){
all_maps[mapName]->health_zones.erase(id);
}
}
string map_get_health_settings_at(string mapName,float x,float y,float z){
if(all_maps.find(mapName)==all_maps.end())return"";
GameMapData* m=all_maps[mapName].get();
for(auto it=m->health_zones.rbegin();it!=m->health_zones.rend();++it){
const auto& zoneList=it->second;
for(int i=zoneList.size()-1;i>=0;i--){
const auto& z_obj=zoneList[i];
if(x>=z_obj.min_x&&x<=z_obj.max_x&&y>=z_obj.min_y&&y<=z_obj.max_y&&z>=z_obj.min_z&&z<=z_obj.max_z){
return to_string(z_obj.interval)+":"+to_string(z_obj.min_val)+":"+to_string(z_obj.max_val);
}
}
}
return"";
}
void map_add_thirst_reset_zone(string mapName,int id,float minx,float maxx,float miny,float maxy,float minz,float maxz){
if(all_maps.find(mapName)==all_maps.end())return;
MapReplenishZone z={id,minx,maxx,miny,maxy,minz,maxz};
all_maps[mapName]->thirst_reset_zones[id].push_back(z);
}
void map_remove_thirst_reset_zone(string mapName,int id){
if(all_maps.find(mapName)!=all_maps.end()){
all_maps[mapName]->thirst_reset_zones.erase(id);
}
}
bool map_is_thirst_reset_at(string mapName,float x,float y,float z){
if(all_maps.find(mapName)==all_maps.end())return false;
GameMapData* m=all_maps[mapName].get();
for(auto it=m->thirst_reset_zones.rbegin();it!=m->thirst_reset_zones.rend();++it){
const auto& zoneList=it->second;
for(int i=zoneList.size()-1;i>=0;i--){
const auto& z_obj=zoneList[i];
if(x>=z_obj.min_x&&x<=z_obj.max_x&&y>=z_obj.min_y&&y<=z_obj.max_y&&z>=z_obj.min_z&&z<=z_obj.max_z){
return true;
}
}
}
return false;
}
void map_add_hunger_reset_zone(string mapName,int id,float minx,float maxx,float miny,float maxy,float minz,float maxz){
if(all_maps.find(mapName)==all_maps.end())return;
MapReplenishZone z={id,minx,maxx,miny,maxy,minz,maxz};
all_maps[mapName]->hunger_reset_zones[id].push_back(z);
}
void map_remove_hunger_reset_zone(string mapName,int id){
if(all_maps.find(mapName)!=all_maps.end()){
all_maps[mapName]->hunger_reset_zones.erase(id);
}
}
bool map_is_hunger_reset_at(string mapName,float x,float y,float z){
if(all_maps.find(mapName)==all_maps.end())return false;
GameMapData* m=all_maps[mapName].get();
for(auto it=m->hunger_reset_zones.rbegin();it!=m->hunger_reset_zones.rend();++it){
const auto& zoneList=it->second;
for(int i=zoneList.size()-1;i>=0;i--){
const auto& z_obj=zoneList[i];
if(x>=z_obj.min_x&&x<=z_obj.max_x&&y>=z_obj.min_y&&y<=z_obj.max_y&&z>=z_obj.min_z&&z<=z_obj.max_z){
return true;
}
}
}
return false;
}
void map_add_climate(string mapName,int id,float minx,float maxx,float miny,float maxy,float minz,float maxz,float min_t,float max_t,bool interior){
if(all_maps.find(mapName)==all_maps.end())return;
MapClimate c;
c.id=id;
c.min_x=minx;c.max_x=maxx;
c.min_y=miny;c.max_y=maxy;
c.min_z=minz;c.max_z=maxz;
c.min_temp=min_t;
c.max_temp=max_t;
c.is_interior=interior;
all_maps[mapName]->climates[id]=c;
}
void map_remove_climate(string mapName,int id){
if(all_maps.find(mapName)!=all_maps.end()){
all_maps[mapName]->climates.erase(id);
}
}
string map_get_climate_at(string mapName,float x,float y,float z){
if(all_maps.find(mapName)==all_maps.end())return"20:25:0";
GameMapData* m=all_maps[mapName].get();
for(auto it=m->climates.rbegin();it!=m->climates.rend();++it){
const auto& c=it->second;
if(x>=c.min_x&&x<=c.max_x&&
y>=c.min_y&&y<=c.max_y&&
z>=c.min_z&&z<=c.max_z){
return to_string(c.min_temp)+":"+to_string(c.max_temp)+":"+(c.is_interior?"1":"0");
}
}
return"20:25:0";
}
bool map_is_interior(string mapName,float x,float y,float z){
if(all_maps.find(mapName)==all_maps.end())return false;
GameMapData* m=all_maps[mapName].get();
for(auto it=m->climates.rbegin();it!=m->climates.rend();++it){
const auto& c=it->second;
if(x>=c.min_x&&x<=c.max_x&&
y>=c.min_y&&y<=c.max_y&&
z>=c.min_z&&z<=c.max_z){
return c.is_interior;
}
}
return false;
}
void map_add_effect(string mapName,int id,float minx,float maxx,float miny,float maxy,float minz,float maxz,string type,string params){
if(all_maps.find(mapName)==all_maps.end())return;
MapFX fx;
fx.id=id;
fx.min_x=minx;fx.max_x=maxx;
fx.min_y=miny;fx.max_y=maxy;
fx.min_z=minz;fx.max_z=maxz;
fx.fx_type=type;
fx.params=params;
all_maps[mapName]->effects[id]=fx;
}
void map_remove_effect(string mapName,int id){
if(all_maps.find(mapName)!=all_maps.end()){
all_maps[mapName]->effects.erase(id);
}
}
string map_get_fx_from_location(string mapName,float x,float y,float z){
if(all_maps.find(mapName)==all_maps.end())return"";
GameMapData* m=all_maps[mapName].get();
for(auto const&[id,fx]:m->effects){
if(x>=fx.min_x&&x<=fx.max_x&&
y>=fx.min_y&&y<=fx.max_y&&
z>=fx.min_z&&z<=fx.max_z){
return fx.fx_type+":"+fx.params;
}
}
return"";
}
string map_find_path(string mapName,int sx,int sy,int sz,int ex,int ey,int ez,string ruleString,int limit){
if(all_maps.find(mapName)==all_maps.end())return"";
GameMapData* m=all_maps[mapName].get();
map<string,int>costs=parse_rules(ruleString);
if(sx<MIN_COORD_LIMIT||sx>MAX_COORD_LIMIT||ex<MIN_COORD_LIMIT||ex>MAX_COORD_LIMIT)return"";
bool ghostMode=(costs.count("ignore_walls")&&costs["ignore_walls"]==1);
unordered_map<unsigned long long,Node*>allNodes;
vector<unique_ptr<Node>>nodeGuard;
priority_queue<Node*,vector<Node*>,CompareNode>openSet;
auto createNode=[&](int x,int y,int z,float g,float h,Node* p)->Node*{
auto n=make_unique<Node>();
n->x=x;n->y=y;n->z=z;
n->g_cost=g;n->h_cost=h;
n->parent=p;
Node* ptr=n.get();
nodeGuard.push_back(move(n));
return ptr;
};
Node* startNode=createNode(sx,sy,sz,0,0,nullptr);
openSet.push(startNode);
allNodes[get_coord_hash(sx,sy,sz)]=startNode;
string resultPath="";
int max_iterations=(limit>0)?limit:15000;
int iter=0;
int dirs[6][3]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
while(!openSet.empty()&&iter<max_iterations){
iter++;
Node* current=openSet.top();
openSet.pop();
if(current->x==ex&&current->y==ey&&current->z==ez){
Node* temp=current;
while(temp!=nullptr){
resultPath=to_string(temp->x)+","+to_string(temp->y)+","+to_string(temp->z)+";"+resultPath;
temp=temp->parent;
}
if(!resultPath.empty()&&resultPath.back()==';')resultPath.pop_back();
break;
}
for(int i=0;i<6;i++){
int nx=current->x+dirs[i][0];
int ny=current->y+dirs[i][1];
int nz=current->z+dirs[i][2];
if(nx<MIN_COORD_LIMIT||nx>MAX_COORD_LIMIT||ny<MIN_COORD_LIMIT||ny>MAX_COORD_LIMIT)continue;
if(nx<m->world_min_x||nx>m->world_max_x||ny<m->world_min_y||ny>m->world_max_y||nz<m->world_min_z||nz>m->world_max_z)continue;
unsigned long long nKey=get_coord_hash(nx,ny,nz);
if(allNodes.count(nKey)){
if(current->g_cost+1>=allNodes[nKey]->g_cost)continue;
}
string tileType="air";
bool isSolidWall=false;
for(int f=TOTAL_FRAMES-1;f>=0;f--){
int size=FRAME_SIZES[f];
unsigned long long chunkKey=get_chunk_hash(nx,ny,nz,size);
if(m->chunks[f].find(chunkKey)!=m->chunks[f].end()){
vector<int>& ids=m->chunks[f][chunkKey];
for(int ii=ids.size()-1;ii>=0;ii--){
int id=ids[ii];
if(m->objects.count(id)){
vector<MapObject>& list=m->objects[id];
for(int j=list.size()-1;j>=0;j--){
const auto& obj=list[j];
if(obj.contains(nx,ny,nz)){
tileType=obj.type;
if(obj.is_wall)isSolidWall=true;
goto found_tile;
}
}
}
}
}
}
found_tile:;
int moveCost=1;
if(isSolidWall){
moveCost=ghostMode?1:WALL_COST;
}else if(costs.count(tileType)){
moveCost=costs[tileType];
}else if(costs.count("default")){
moveCost=costs["default"];
}
if(moveCost>=WALL_COST)continue;
float newGCost=current->g_cost+moveCost;
if(allNodes.count(nKey)){
Node* neighbor=allNodes[nKey];
if(newGCost<neighbor->g_cost){
neighbor->g_cost=newGCost;
neighbor->parent=current;
openSet.push(neighbor);
}
}else{
float h=abs(nx-ex)+abs(ny-ey)+abs(nz-ez);
Node* neighbor=createNode(nx,ny,nz,newGCost,h,current);
allNodes[nKey]=neighbor;
openSet.push(neighbor);
}
}
}
return resultPath;
}
void map_set_min_x(string mapName,float val){
if(all_maps.find(mapName)!=all_maps.end()){
all_maps[mapName]->world_min_x=val;
}
}
void map_set_max_x(string mapName,float val){
if(all_maps.find(mapName)!=all_maps.end()){
all_maps[mapName]->world_max_x=val;
}
}
void map_set_min_y(string mapName,float val){
if(all_maps.find(mapName)!=all_maps.end()){
all_maps[mapName]->world_min_y=val;
}
}
void map_set_max_y(string mapName,float val){
if(all_maps.find(mapName)!=all_maps.end()){
all_maps[mapName]->world_max_y=val;
}
}
void map_set_min_z(string mapName,float val){
if(all_maps.find(mapName)!=all_maps.end()){
all_maps[mapName]->world_min_z=val;
}
}
void map_set_max_z(string mapName,float val){
if(all_maps.find(mapName)!=all_maps.end()){
all_maps[mapName]->world_max_z=val;
}
}
float map_get_min_x(string mapName){
if(all_maps.find(mapName)!=all_maps.end()){
return all_maps[mapName]->world_min_x;
}
return 0.0f;
}
float map_get_max_x(string mapName){
if(all_maps.find(mapName)!=all_maps.end()){
return all_maps[mapName]->world_max_x;
}
return 0.0f;
}
float map_get_min_y(string mapName){
if(all_maps.find(mapName)!=all_maps.end()){
return all_maps[mapName]->world_min_y;
}
return 0.0f;
}
float map_get_max_y(string mapName){
if(all_maps.find(mapName)!=all_maps.end()){
return all_maps[mapName]->world_max_y;
}
return 0.0f;
}
float map_get_min_z(string mapName){
if(all_maps.find(mapName)!=all_maps.end()){
return all_maps[mapName]->world_min_z;
}
return 0.0f;
}
float map_get_max_z(string mapName){
if(all_maps.find(mapName)!=all_maps.end()){
return all_maps[mapName]->world_max_z;
}
return 0.0f;
}
plugin_main(nvgt_plugin_shared* shared){
prepare_plugin(shared);
shared->script_engine->RegisterObjectType("vector3",sizeof(MapVector3),asOBJ_VALUE|asOBJ_POD|asOBJ_APP_CLASS_CA);
shared->script_engine->RegisterObjectProperty("vector3","float x",asOFFSET(MapVector3,x));
shared->script_engine->RegisterObjectProperty("vector3","float y",asOFFSET(MapVector3,y));
shared->script_engine->RegisterObjectProperty("vector3","float z",asOFFSET(MapVector3,z));
shared->script_engine->RegisterObjectBehaviour("vector3",asBEHAVE_CONSTRUCT,"void f()",asFUNCTION(ConstructVector3),asCALL_CDECL_OBJLAST);
shared->script_engine->RegisterObjectBehaviour("vector3",asBEHAVE_CONSTRUCT,"void f(float, float, float)",asFUNCTION(ConstructVector3Init),asCALL_CDECL_OBJLAST);
shared->script_engine->RegisterGlobalFunction("void map_create(string name)",asFUNCTION(map_create),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("bool map_delete(string name)",asFUNCTION(map_delete),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("void map_reset_system()",asFUNCTION(map_reset_system),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("void map_set_center(string map, float x, float y, float z)",asFUNCTION(map_set_center),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("float map_get_center_x(string map)",asFUNCTION(map_get_center_x),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("float map_get_center_y(string map)",asFUNCTION(map_get_center_y),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("float map_get_center_z(string map)",asFUNCTION(map_get_center_z),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("void map_set_owner(string map, string owner)",asFUNCTION(map_set_owner),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("string map_get_owner(string map)",asFUNCTION(map_get_owner),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("void map_set_description(string map, string desc)",asFUNCTION(map_set_description),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("string map_get_description(string map)",asFUNCTION(map_get_description),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("void map_set_storage(string map, string data)",asFUNCTION(map_set_storage),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("string map_get_storage(string map)",asFUNCTION(map_get_storage),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("void map_set_hours(string map, int open, int close)",asFUNCTION(map_set_hours),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("int map_get_open_hour(string map)",asFUNCTION(map_get_open_hour),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("int map_get_close_hour(string map)",asFUNCTION(map_get_close_hour),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("string map_get_all_map_names(string delimiter=\"|\")",asFUNCTION(map_get_all_map_names),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("vector3 map_get_min_value(string map)",asFUNCTION(map_get_min_value),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("vector3 map_get_max_value(string map)",asFUNCTION(map_get_max_value),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("void map_add_tile(string map, float minx, float maxx, float miny, float maxy, float minz, float maxz, string type, int id, bool trackable=false, bool stair=true)",asFUNCTION(map_add_tile),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("void map_add_wall(string map, float minx, float maxx, float miny, float maxy, float minz, float maxz, string type, int id)",asFUNCTION(map_add_wall),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("void map_remove_tile_from_id(string map, int id)",asFUNCTION(map_remove_tile_from_id),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("string map_get_tile_at(string map, float x, float y, float z)",asFUNCTION(map_get_tile_at),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("string map_get_tile_index_from_id(string map, int id)",asFUNCTION(map_get_tile_index_from_id),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("string map_get_tile_list(string map, float px, float py, float pz, int dist, string parse=\":\")",asFUNCTION(map_get_tile_list),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("bool map_is_stair(string map, float x, float y, float z)",asFUNCTION(map_is_stair),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("void map_add_zone(string map, float minx, float maxx, float miny, float maxy, float minz, float maxz, int id, string text, bool trackable=false)",asFUNCTION(map_add_zone),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("void map_remove_zone(string map, int id)",asFUNCTION(map_remove_zone),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("string map_get_zone_at(string map, float x, float y, float z)",asFUNCTION(map_get_zone_at),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("string map_get_zone_list(string map)",asFUNCTION(map_get_zone_list),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("void map_add_safe_zone(string map, int id, float minx, float maxx, float miny, float maxy, float minz, float maxz)",asFUNCTION(map_add_safe_zone),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("void map_remove_safe_zone(string map, int id)",asFUNCTION(map_remove_safe_zone),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("bool map_is_safe(string map, float x, float y, float z)",asFUNCTION(map_is_safe),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("void map_add_restricted_zone(string map, int id, float minx, float maxx, float miny, float maxy, float minz, float maxz, string items)",asFUNCTION(map_add_restricted_zone),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("void map_remove_restricted_zone(string map, int id)",asFUNCTION(map_remove_restricted_zone),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("bool map_can_use_item(string map, float x, float y, float z, string item)",asFUNCTION(map_can_use_item),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("void map_add_climate(string map, int id, float minx, float maxx, float miny, float maxy, float minz, float maxz, float min_t, float max_t, bool interior)",asFUNCTION(map_add_climate),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("void map_remove_climate(string map, int id)",asFUNCTION(map_remove_climate),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("string map_get_climate_at(string map, float x, float y, float z)",asFUNCTION(map_get_climate_at),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("bool map_is_interior(string map, float x, float y, float z)",asFUNCTION(map_is_interior),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("string map_find_path(string map, int sx, int sy, int sz, int ex, int ey, int ez, string rules, int limit=15000)",asFUNCTION(map_find_path),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("void map_add_effect(string map, int id, float minx, float maxx, float miny, float maxy, float minz, float maxz, string type, string params)",asFUNCTION(map_add_effect),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("void map_remove_effect(string map, int id)",asFUNCTION(map_remove_effect),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("string map_get_fx_from_location(string map, float x, float y, float z)",asFUNCTION(map_get_fx_from_location),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("void map_add_thirst_zone(string map, int id, float minx, float maxx, float miny, float maxy, float minz, float maxz, int interval, int min_val, int max_val)",asFUNCTION(map_add_thirst_zone),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("void map_remove_thirst_zone(string map, int id)",asFUNCTION(map_remove_thirst_zone),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("string map_get_thirst_settings_at(string map, float x, float y, float z)",asFUNCTION(map_get_thirst_settings_at),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("void map_add_hunger_zone(string map, int id, float minx, float maxx, float miny, float maxy, float minz, float maxz, int interval, int min_val, int max_val)",asFUNCTION(map_add_hunger_zone),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("void map_remove_hunger_zone(string map, int id)",asFUNCTION(map_remove_hunger_zone),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("string map_get_hunger_settings_at(string map, float x, float y, float z)",asFUNCTION(map_get_hunger_settings_at),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("void map_add_health_zone(string map, int id, float minx, float maxx, float miny, float maxy, float minz, float maxz, int interval, int min_val, int max_val)",asFUNCTION(map_add_health_zone),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("void map_remove_health_zone(string map, int id)",asFUNCTION(map_remove_health_zone),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("string map_get_health_settings_at(string map, float x, float y, float z)",asFUNCTION(map_get_health_settings_at),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("void map_add_thirst_reset_zone(string map, int id, float minx, float maxx, float miny, float maxy, float minz, float maxz)",asFUNCTION(map_add_thirst_reset_zone),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("void map_remove_thirst_reset_zone(string map, int id)",asFUNCTION(map_remove_thirst_reset_zone),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("bool map_is_thirst_reset_at(string map, float x, float y, float z)",asFUNCTION(map_is_thirst_reset_at),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("void map_add_hunger_reset_zone(string map, int id, float minx, float maxx, float miny, float maxy, float minz, float maxz)",asFUNCTION(map_add_hunger_reset_zone),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("void map_remove_hunger_reset_zone(string map, int id)",asFUNCTION(map_remove_hunger_reset_zone),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("bool map_is_hunger_reset_at(string map, float x, float y, float z)",asFUNCTION(map_is_hunger_reset_at),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("void map_set_min_x(string map, float val)",asFUNCTION(map_set_min_x),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("void map_set_max_x(string map, float val)",asFUNCTION(map_set_max_x),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("void map_set_min_y(string map, float val)",asFUNCTION(map_set_min_y),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("void map_set_max_y(string map, float val)",asFUNCTION(map_set_max_y),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("void map_set_min_z(string map, float val)",asFUNCTION(map_set_min_z),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("void map_set_max_z(string map, float val)",asFUNCTION(map_set_max_z),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("float map_get_min_x(string map)",asFUNCTION(map_get_min_x),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("float map_get_max_x(string map)",asFUNCTION(map_get_max_x),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("float map_get_min_y(string map)",asFUNCTION(map_get_min_y),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("float map_get_max_y(string map)",asFUNCTION(map_get_max_y),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("float map_get_min_z(string map)",asFUNCTION(map_get_min_z),asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("float map_get_max_z(string map)",asFUNCTION(map_get_max_z),asCALL_CDECL);
return true;
}