#include <ros/ros.h>
#include <ros/console.h>
#include <vector>
#include <map>
#include <regex>
#include <algorithm>
#include <chrono>
#include <thread>
#include <sync_params/ParameterMsg.h>

/* Nick Sullivan, The University of Adelaide, 2018.
   This file periodically reads from the parameter server, publishes it to a 
   global topic, then writes to the parameter server. It's intended to work
   accross multiple masters in multi_master_fkie setups.
   
   The parameter server is read in as an XmlRpcValue, turned into an XML string
   to be published, then restored into an XmlRpcValue.
*/

using namespace std;

bool debug;                         //extra printouts if true
bool use_cpp_time;                  //explanation in 'main'
bool use_death_timer;               //if node should die after some time
double death_timer;                 //seconds
double rate;                        //parameter poll rate
ros::NodeHandle* n_ptr;             //to write to the parameter server
map<string, string> param_map;      //stores parameters we've seen already

vector<string> blacklist;
vector<string> whitelist;

ros::Publisher  param_pub;
ros::Subscriber param_sub;

/**************************************************
 * Helper functions
 **************************************************/

// From https://stackoverflow.com/questions/20406744
// /how-to-find-and-replace-all-occurrences-of-a-substring-in-a-string
std::string replaceAll( std::string const& original, 
                        std::string const& from, 
                        std::string const& to ) {
  std::string results;
  std::string::const_iterator end = original.end();
  std::string::const_iterator current = original.begin();
  std::string::const_iterator next = std::search( current, end, from.begin(), from.end() );
  while ( next != end ) {
    results.append( current, next );
    results.append( to );
    current = next + from.size();
    next = std::search( current, end, from.begin(), from.end() );
  }
  results.append( current, next );
  return results;
}

/**************************************************
 * Whitelist/Blacklist functions
 **************************************************/
 
bool isOnBlackList( string key ){
  ROS_DEBUG_STREAM("Blacklist check: " << key);
  for( int i=0; i< blacklist.size(); i++ ){
    std::regex reg(blacklist[i]);
    if( regex_match(key, reg) ){
      ROS_DEBUG_STREAM(" TRUE ");;
      return true;
    }
  }
  ROS_DEBUG_STREAM(" FALSE ");
  return false;
}

bool isOnWhiteList( string key ){
  ROS_DEBUG_STREAM("Whitelist check: " << key);
  for( int i=0; i< whitelist.size(); i++ ){
    std::regex reg(whitelist[i]);
    if( regex_match(key, reg) ){
      ROS_DEBUG_STREAM(" TRUE " <<endl);
      return true;
    }
  }
  if(debug) cout << " FALSE " <<endl;
  return false;
}
 
/**************************************************
 * Publisher functions
 **************************************************/

void publishParam(sync_params::ParameterMsg param){
  ROS_DEBUG_STREAM("Publishing: " << param.key);
  param_pub.publish(param);
} 
 
/**************************************************
 * Subscriber functions
 **************************************************/

void subscribeParam(const sync_params::ParameterMsg::ConstPtr& msg) {
  string key = msg->key;
  string xml = msg->xml;
  if( isOnBlackList(key) && !isOnWhiteList(key) ) return;
  bool new_entry = param_map.insert( make_pair(key,xml) ).second;
  if( !new_entry ){
    return;
  }
  ROS_DEBUG_STREAM("SyncParams: Synchronising parameter [" << key << "]");
  int offset = 0;
  string const &xml2 = xml;
  XmlRpc::XmlRpcValue v(xml2, &offset);
  n_ptr->setParam(key, v);
}

/**************************************************
 * Initialising functions
 **************************************************/
 
void loadSubs(ros::NodeHandle n){
  param_sub = n.subscribe("/params", 100, subscribeParam);                                            
}

void loadPubs(ros::NodeHandle n){
  param_pub = n.advertise<sync_params::ParameterMsg>("/params", 100);
}

void loadParams(ros::NodeHandle n_priv){
  XmlRpc::XmlRpcValue v;
  string s;
  if( n_priv.getParam("whitelist", v) ){
    ROS_ASSERT(v.getType() == XmlRpc::XmlRpcValue::TypeArray);
    for(int i=0; i < v.size(); i++) {
      s = replaceAll( v[i] , "*", ".*");
      whitelist.push_back(s);
    }
  }
  if( n_priv.getParam("blacklist", v) ){
    ROS_ASSERT(v.getType() == XmlRpc::XmlRpcValue::TypeArray);
    for(int i=0; i < v.size(); i++) {
      s = replaceAll( v[i] , "*", ".*");
      blacklist.push_back(s);
    }
  }
  n_priv.param("debug", debug, false);
  n_priv.param("use_cpp_time", use_cpp_time, false);
  n_priv.param("rate", rate, 1.0);
  n_priv.param("death_timer", death_timer, -1.0);
  use_death_timer = false;
  if( death_timer > 0.0 ){
    use_death_timer = true;
  }
}

/**************************************************
 * Main
 **************************************************/
 
// We might not use the in-built ROS rate because deadlock can occur for gazebo
// spawning. Basically Gazebo pauses the clock when loading a robot, and it 
// waits for the robot_description parameter. But that parameter never gets
// synced without the clock going.
int main(int argc, char** argv){
  ros::init(argc, argv, "sync_params");
  ros::NodeHandle n;
  ros::NodeHandle n_priv("~");
  n_ptr = &n;
  loadPubs(n);
  loadSubs(n);
  loadParams(n_priv);
  
  string key;
  XmlRpc::XmlRpcValue v;
  sync_params::ParameterMsg msg;
  vector<string> param_keys;
  ros::Time start_time = ros::Time::now();
  bool dead = false;
  
  ros::Rate r(rate);
  while( ros::ok() ){
    // For each key, add it to the map and publish.
    n.getParamNames(param_keys);
    for( int i=0; i<param_keys.size(); i++ ){
      key = param_keys[i];
      if( isOnBlackList(key) && !isOnWhiteList(key) ) continue;
      n_ptr->getParam(key, v);
      msg.key = key;
      msg.xml = v.toXml();
      publishParam(msg);
    }
    // Handle callbacks.
    ros::spinOnce();
    if( use_cpp_time ){
      this_thread::sleep_for( chrono::milliseconds((int)(1000/rate)) );
    } else {
      r.sleep();
    }
    if( use_death_timer && ros::Time::now()-start_time > ros::Duration(death_timer) ){
      ROS_INFO("SyncParams has finished polling.");
      ros::spin();
    }
  }
  return 0;
};
