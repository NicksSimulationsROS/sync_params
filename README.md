# sync_params
This package periodically reads from the parameter server, publishes it to a 
global topic, then writes to the parameter server. It's intended to work
accross multiple masters in [`multimaster_fkie`](http://wiki.ros.org/multimaster_fkie) setups.

Only new parameters will be written to the parameter server. No parameter
will ever be overwritten.

By default, all parameters will be synchronised. The `blacklist` excludes parameters
based on their name. The `whitelist` parameters are exempt from the `blacklist`. 
These can both be regular expressions. For example:
```
blacklist = ["/*"]
whitelist = [/my_parameter]
```
Would only synchronise `my_parameter`.

The parameter `use_cpp_time` is designed for spawning robots in Gazebo. Gazebo pauses
the ROS clock when loading a robot, and waits for the `robot_description` parameter.
But that parameter might not get synchronised if `sync_params` uses the ROS clock. 

## 1. Nodes
### 1.1 sync_params_node
#### 1.1.1 Subscribed Topics
`/params` ([`sync_params/ParameterMsg`](msg/ParameterMsg.msg))

The parameters to be synced.

#### 1.1.2 Published Topics
/params ([`sync_params/ParameterMsg`](msg/ParameterMsg.msg))

The parameters to be synced.

#### 1.1.3 Parameters
`~debug` (`boolean`, default: `False`)

Whether to print out lots of information

`~rate` (`double`, default: `1.0`)

The rate to read/publish/write parameters

`~death_timer` (`double`, default `-1.0`)

How many ROS seconds until publishing stops

`~use_cpp_time` (`boolean`, default: `False`)

Whether to use computer clock instead of ROS clock. Does not affect `death_timer`

`~blacklist` (`array`, default="`[]`")

List of parameter names to exclude from synchronising

`~whitelist` (`array`, default="`[]`")

List of parameter names to exclude from the `blacklist`

