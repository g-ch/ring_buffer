#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include "opencv2/core.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"
#include "ros/ros.h"

#include <pcl/point_types.h>
#include <pcl_conversions/pcl_conversions.h>

//#include <cv.h>
//#define ADDR "192.168.1.144"
#define ADDR "127.0.0.1"  //在本机测试用这个地址，如果连接其他电脑需要更换IP

#define SERVERPORT 10001
#define BYTE_SEND_INTERVAL 1

using namespace std;
using namespace cv;
int sock_clit;
struct sockaddr_in serv_addr;

int send_data(char *data)  // 255k at most
{
    /*send mark*/
    char buffer[8];
    buffer[0] = 's';
    buffer[1] = 't';
    buffer[2] = 'a';
    buffer[3] = 'r';
    buffer[4] = 't';
    buffer[5] = *data;
    buffer[6] = *(data + 1);
    buffer[7] = *(data + 2);

    sendto(sock_clit, buffer, 8, 0, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr));
    waitKey(BYTE_SEND_INTERVAL);

    return 1;
}

/* @function main */
int main(int argc, char **argv)
{
    ros::init(argc, argv, "socket_client");
    ros::NodeHandle n;

    // publisher to rviz
    ros::Publisher cloud_pub = n.advertise<sensor_msgs::PointCloud2>("client/cloud", 1);

    /*compress paras init*/
    vector<int> param = vector<int>(2);
    param[0] = CV_IMWRITE_JPEG_QUALITY;
    param[1] = 50;  // default(95) 0-100

    /*socket paras init*/
    char get_msg[10] = { 0 };

    // sock = socket(AF_INET, SOCK_STREAM, 0);
    sock_clit = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in c_in;
    socklen_t len = sizeof(struct sockaddr);

    if(sock_clit == -1)
    {
        return -1;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ADDR);  // 注释1
    serv_addr.sin_port = htons(SERVERPORT);
    if(connect(sock_clit, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
    {  // 注释2
        cout << "connect error\n";
        return -1;
    }
    else
    {
        cout << "connected ...\n" << endl;  //注释3
    }

    cout << "transmission started" << endl;

    char quit_flag = ' ';
    double buf[1024];  // content buff area

    while(ros::ok())
    {
        char send_buff[8] = "";
        send_buff[0] = 1;
        send_buff[1] = 2;
        send_buff[2] = 3;
        send_data(send_buff);

        /*recieve*/
        int r_len = recvfrom(sock_clit, buf, 1024, 0, (struct sockaddr *)&c_in, &len);

        if(r_len > 0)
        {
            cout << "cloud received!" << endl;
            /*Verify first package*/
            if(buf[0] == 1 && buf[1] == 1 && buf[2] == 1)
            {
                int point_num = buf[4];
                cout << "point num:" << point_num << endl;
                int pkg_num = buf[5];
                int last_pkg_bytes = buf[6] * 256 + buf[7];
                vector<double> rec_vec;

                long int total_size = (pkg_num - 1) * 1024 + last_pkg_bytes;
                cout << "received data length " << total_size << endl;
                rec_vec.resize(total_size);

                /*receive and reconstruct data*/
                for(int i = 0; i < pkg_num; i++)
                {
                    r_len = recvfrom(sock_clit, buf, 1024, 0, (struct sockaddr *)&c_in, &len);
                    int pkg_size = 1024;
                    if(i == pkg_num - 1 && last_pkg_bytes != 0) pkg_size = last_pkg_bytes;

                    // cout <<"pkg_size "<<pkg_size<<endl;

                    for(int j = 0; j < pkg_size; j++) rec_vec[1024 * i + j] = buf[j];
                }

                /*decode to pcl cloud*/
                cout << " received vect: " << endl;
                for(int i = 0; i < point_num * 3; ++i)
                {
                    cout << i << " , " << rec_vec[i] << endl;
                    if(i % 3 == 2) cout << endl;
                }
                if(!rec_vec.size() == 0)
                {
                    pcl::PointCloud<pcl::PointXYZ> cloud;
                    for(int i = 0; i < point_num; i++)
                    {
                        pcl::PointXYZ p;
                        p.x = rec_vec.at(3 * i);
                        p.y = rec_vec.at(3 * i + 1);
                        p.z = rec_vec.at(3 * i + 2);
                        cloud.points.push_back(p);
                    }
                    // publish to rviz
                    sensor_msgs::PointCloud2 cloud2;
                    pcl::toROSMsg(cloud, cloud2);
                    cloud2.header.frame_id = "world";
                    cloud_pub.publish(cloud2);
                    cout << "cloud publish to rviz!" << endl;
                }

                quit_flag = waitKey(10);
            }
        }
        // if(!input_frame.empty())imshow("client", input_frame);
        // quit_flag = waitKey(20);

        ros::spinOnce();
    }

    destroyWindow("client");
    close(sock_clit);

    return 0;
}