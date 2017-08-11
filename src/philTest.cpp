#include <ros/ros.h>
#include <std_msgs/Bool.h>

#include "server_http.hpp"
#include "client_http.hpp"

//Added for the json-example
#define BOOST_SPIRIT_THREADSAFE
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

//Added for the default_resource example
#include <fstream>
#include <boost/filesystem.hpp>
#include <vector>
#include <algorithm>
#ifdef HAVE_OPENSSL
#include "crypto.hpp"
#endif

using namespace std;
//Added for the json-example:
using namespace boost::property_tree;

typedef SimpleWeb::Server<SimpleWeb::HTTP> HttpServer;

//Added for the default_resource example
void default_resource_send(const HttpServer &server, const shared_ptr<HttpServer::Response> &response,
                           const shared_ptr<ifstream> &ifs);

std_msgs::Bool button_msg;

void button_cb(const std_msgs::Bool::ConstPtr& button_state){
    button_msg = *button_state;
    //ROS_WARN_STREAM("button state is: " << (bool)button_state->data);
}

int main(int argc, char **argv) {
    ros::init(argc, argv, "reward_display_node");
    ros::NodeHandle n;

    //ros::Subscriber button_sub = n.subscribe<std_msgs::Bool>("/babbling_module/ButtonModule_f8_f0_05_f5_cf_96/led", 1, button_cb);
    ros::Subscriber button_sub = n.subscribe<std_msgs::Bool>("/babbling_module/ButtonModule_f8_f0_05_f5_cf_96/button_pressed", 1, button_cb);

    ros::AsyncSpinner my_spinner(4);
    my_spinner.start();
    //HTTP-server at port 8080 using 1 thread
    //Unless you do more heavy non-threaded processing in the resources,
    //1 thread is usually faster than several threads
    HttpServer server;
    server.config.port=8080;

    //POST-example for the path /json, responds firstName+" "+lastName from the posted json
    //Responds with an appropriate error message if the posted json is not valid, or if firstName or lastName is missing
    //Example posted json:
    //{
    //  "firstName": "John",
    //  "lastName": "Smith",
    //  "age": 25
    //}
    server.resource["^/state$"]["GET"]=[](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> ) {
        try {
            stringstream content_stream;
            content_stream << "{\"state\":\"";
            if(button_msg.data)
                content_stream << "yes";
            else
                content_stream << "no";
            content_stream << "\",\"img\":\"";
            if(button_msg.data)
                content_stream << "button_yes.png";
            else
                content_stream << "button_no.png";
            content_stream << "\"}";
            content_stream.seekp(0, ios::end);
            *response << "HTTP/1.1 200 OK\r\n"
                      << "Content-Type: application/json\r\n"
                      << "Content-Length: " << content_stream.tellp() << "\r\n\r\n"
                      << content_stream.rdbuf();
        }
        catch(exception& e) {
            *response << "HTTP/1.1 400 Bad Request\r\nContent-Length: " << strlen(e.what()) << "\r\n\r\n" << e.what();
        }
    };

    //Default GET-example. If no other matches, this anonymous function will be called.
    //Will respond with content in the web/-directory, and its subdirectories.
    //Default file: index.html
    //Can for instance be used to retrieve an HTML 5 client that uses REST-resources on this server
    server.default_resource["GET"]=[&server](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
        try {
            auto web_root_path=boost::filesystem::canonical("web");
            auto path=boost::filesystem::canonical(web_root_path/request->path);
            //Check if path is within web_root_path
            if(distance(web_root_path.begin(), web_root_path.end())>distance(path.begin(), path.end()) ||
                    !equal(web_root_path.begin(), web_root_path.end(), path.begin()))
                throw invalid_argument("path must be within root path");
            if(boost::filesystem::is_directory(path))
                path/="index.html";
            if(!(boost::filesystem::exists(path) && boost::filesystem::is_regular_file(path)))
                throw invalid_argument("file does not exist");

            std::string cache_control, etag;

            // Uncomment the following line to enable Cache-Control
            // cache_control="Cache-Control: max-age=86400\r\n";

            auto ifs=make_shared<ifstream>();
            ifs->open(path.string(), ifstream::in | ios::binary | ios::ate);

            if(*ifs) {
                auto length=ifs->tellg();
                ifs->seekg(0, ios::beg);

                *response << "HTTP/1.1 200 OK\r\n" << cache_control << etag << "Content-Length: " << length << "\r\n\r\n";
                default_resource_send(server, response, ifs);
            }
            else
                throw invalid_argument("could not read file");
        }
        catch(const exception &e) {
            string content="Could not open path "+request->path+": "+e.what();
            *response << "HTTP/1.1 400 Bad Request\r\nContent-Length: " << content.length() << "\r\n\r\n" << content;
        }
    };


    thread server_thread([&server](){
        //Start server
        cout << "Server started on port #" <<server.config.port << " ..."<<endl;
        server.start();

    });

    while(ros::ok()){
        //Wait for server to start so that the client can connect
        this_thread::sleep_for(chrono::milliseconds(10));
        ros::spinOnce();
    }


    return 0;
}

void default_resource_send(const HttpServer &server, const shared_ptr<HttpServer::Response> &response,
                           const shared_ptr<ifstream> &ifs) {
    //read and send 128 KB at a time
    static vector<char> buffer(131072); // Safe when server is running on one thread
    streamsize read_length;
    if((read_length=ifs->read(&buffer[0], buffer.size()).gcount())>0) {
        response->write(&buffer[0], read_length);
        if(read_length==static_cast<streamsize>(buffer.size())) {
            server.send(response, [&server, response, ifs](const boost::system::error_code &ec) {
                if(!ec)
                    default_resource_send(server, response, ifs);
                else
                    cerr << "Connection interrupted" << endl;
            });
        }
    }
}

