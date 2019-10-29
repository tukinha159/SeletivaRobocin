#include <QtNetwork>
#include <stdio.h>
#include "net/robocup_ssl_client.h"
#include "net/grSim_client.h"
#include "util/timer.h"
#include<bits/stdc++.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<sys/socket.h>
#include<unistd.h>
#include<string.h>
#include<stdlib.h>
#include<termios.h>
#include<thread>
#include<chrono>
#include<time.h>
#include "pb/messages_robocup_ssl_detection.pb.h"
#include "pb/messages_robocup_ssl_geometry.pb.h"
#include "pb/messages_robocup_ssl_wrapper.pb.h"
#include "pb/grSim_Packet.pb.h"
#include "pb/grSim_Commands.pb.h"
#include "pb/grSim_Replacement.pb.h"
#include <iostream>
#include "Robos.hpp"
#include <Eigen/Dense>


int main(int argc, char *argv[]){
    (void)argc;
    (void)argv;
    RoboCupSSLClient client;
    client.open(true);
    SSL_WrapperPacket packet;

    GrSim_Client grSim_client;

    //Declarando os parametros utilizados nas manipulações de dados
    Cont_Indice_blue=0;
    Cont_Indice_yellow=0;
    std::Vector<Robos> Blue[8];
    std::Vector<Robos> Yellow[8];
  

    while(true) {
        if (client.receive(packet)) {
            printf("-----Received Wrapper Packet---------------------------------------------\n");
            //see if the packet contains a robot detection frame:
            if (packet.has_detection()) {
                SSL_DetectionFrame detection = packet.detection();
                //Display the contents of the robot detection results:
                double t_now = GetTimeSec();

                printf("-[Detection Data]-------\n");
                //Frame info:
                printf("Camera ID=%d FRAME=%d T_CAPTURE=%.4f\n",detection.camera_id(),detection.frame_number(),detection.t_capture());

                printf("SSL-Vision Processing Latency                   %7.3fms\n",(detection.t_sent()-detection.t_capture())*1000.0);
                printf("Network Latency (assuming synched system clock) %7.3fms\n",(t_now-detection.t_sent())*1000.0);
                printf("Total Latency   (assuming synched system clock) %7.3fms\n",(t_now-detection.t_capture())*1000.0);
                int balls_n = detection.balls_size();
                int robots_blue_n =  detection.robots_blue_size();
                int robots_yellow_n =  detection.robots_yellow_size();

                //Ball info:
                for (int i = 0; i < balls_n; i++) {
                    SSL_DetectionBall ball = detection.balls(i);
                    printf("-Ball (%2d/%2d): CONF=%4.2f POS=<%9.2f,%9.2f> ", i+1, balls_n, ball.confidence(),ball.x(),ball.y());
                    if (ball.has_z()) {
                        printf("Z=%7.2f ",ball.z());
                    } else {
                        printf("Z=N/A   ");
                    }
                    printf("RAW=<%8.2f,%8.2f>\n",ball.pixel_x(),ball.pixel_y());
                }

                //Blue robot info:

                //loop de verificações e manipulação robos azuis
                for (int i = 0; i < robots_blue_n; i++) {
                    SSL_DetectionRobot robot = detection.robots_blue(i);
                    if (robot.has_robot_id()) {
                            
                        if(robot.robot_id >= Cont_Indice_blue ){
                            Blue[Cont_Indice_blue] = new Robos(robot);
                            Cont_Indice_blue++;
                        }
                        else{
                            bool end = false;
                            for(int J=0; J < Cont_Indice_blue && !end; J++){
                                end = Blue[J].Verificar(robot);
                            }
                            if(!end){
                                Blue[Cont_Indice_blue] = new Robos(robot);
                                Cont_Indice_blue++;
                            }
                        }
                        
                    }

                        printf("-Robot(B) (%2d/%2d): ",i+1, robots_blue_n);

                }
                
                // definir robos a serem mostrados em tela
                for(int J=0; J < Cont_Indice_blue; J++){
                    if(Blue[J].ATIVO()){
                        std::thread T1(Blue[J].kalman());
                        std::thread T2(Blue[J].Perda());
                        T1.join();
                        Blue[J].printRobotInfo();

                    }
                
                    if(Blue[J].getx() <= 0){
                        grSim_client.sendCommand(1.0, J);
                    }else{
                        grSim_client.sendCommand(-1.0, J);
                    }
                }

                
                //....................................................

                //Yellow robot info:

                //loop de verificações e manipulação robos amarelos
                for (int i = 0; i < robots_yellow_n; i++) {
                    SSL_DetectionRobot robot = detection.robots_Yellow(i);
                    if (robot.has_robot_id()) {
                            
                        if(robot.robot_id >= Cont_Indice_yellow ){
                            Yellow[Cont_Indice_yellow] = new Robos(robot);
                        }
                        else{
                            bool end = false;
                            for(int J=0; J <Cont_Indice_yellow && !end; J++){
                                end = Yellow[J].Verificar(robot);
                            }
                            if(!end){
                                Yellow[Cont_Indice_yellow] = new Robos(robot);
                                Cont_Indice_yellow++;
                            }
                        }   
                    }
                        printf("-Robot(B) (%2d/%2d): ",i+1, robots_Yellow_n);

                }
                
                // definir robos a serem mostrados em tela
                for(int J=0; J < Cont_Indice_yellow; J++){
                    if(Yellow[J].ATIVO()){
                        std::thread T1(Yellow[J].kalman());
                        std::thread T2(Yellow[J].Perda());
                        T1.join();
                        Yellow[J].printRobotInfo();

                    }
                    if(Yellow[J].getx() <= 0){
                        grSim_client.sendCommand(1.0, J);
                    }else{
                        grSim_client.sendCommand(-1.0, J);
                    }
                }
                //....................................................

            //see if packet contains geometry data:
            if (packet.has_geometry()) {
                const SSL_GeometryData & geom = packet.geometry();
                printf("-[Geometry Data]-------\n");

                const SSL_GeometryFieldSize & field = geom.field();
                printf("Field Dimensions:\n");
                printf("  -field_length=%d (mm)\n",field.field_length());
                printf("  -field_width=%d (mm)\n",field.field_width());
                printf("  -boundary_width=%d (mm)\n",field.boundary_width());
                printf("  -goal_width=%d (mm)\n",field.goal_width());
                printf("  -goal_depth=%d (mm)\n",field.goal_depth());
                printf("  -field_lines_size=%d\n",field.field_lines_size());
                printf("  -field_arcs_size=%d\n",field.field_arcs_size());

                int calib_n = geom.calib_size();
                for (int i=0; i< calib_n; i++) {
                    const SSL_GeometryCameraCalibration & calib = geom.calib(i);
                    printf("Camera Geometry for Camera ID %d:\n", calib.camera_id());
                    printf("  -focal_length=%.2f\n",calib.focal_length());
                    printf("  -principal_point_x=%.2f\n",calib.principal_point_x());
                    printf("  -principal_point_y=%.2f\n",calib.principal_point_y());
                    printf("  -distortion=%.2f\n",calib.distortion());
                    printf("  -q0=%.2f\n",calib.q0());
                    printf("  -q1=%.2f\n",calib.q1());
                    printf("  -q2=%.2f\n",calib.q2());
                    printf("  -q3=%.2f\n",calib.q3());
                    printf("  -tx=%.2f\n",calib.tx());
                    printf("  -ty=%.2f\n",calib.ty());
                    printf("  -tz=%.2f\n",calib.tz());

                    if (calib.has_derived_camera_world_tx() && calib.has_derived_camera_world_ty() && calib.has_derived_camera_world_tz()) {
                      printf("  -derived_camera_world_tx=%.f\n",calib.derived_camera_world_tx());
                      printf("  -derived_camera_world_ty=%.f\n",calib.derived_camera_world_ty());
                      printf("  -derived_camera_world_tz=%.f\n",calib.derived_camera_world_tz());
                    }

                }
            }
        }
    }

    return 0;
}