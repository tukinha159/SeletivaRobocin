#include "Bibliotecas.h"
#include <QtNetwork>
#include "Robos/Robos.h"
#include "Ball/Ball.h"
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
    Cont_Indice_Ball=0;
    std::Vector<Robos> Blue[20];
    std::Vector<Robos> Yellow[20];
    std::Vector<Ball> Bola[20];

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

                #pragma omp parallel{
                    #pragma omp sections{
                        #pragma omp section{
                            //Ball info:
                            for (int i = 0; i < balls_n; i++) {
                                int aux;
                                SSL_DetectionBall ball = detection.balls(i);
                                if(ball_has_area()){
                                    bool end = false;
                                    for(int J=0; J < Cont_Indice_Ball && !end; J++){
                                        end = Bola[J].Verificar(ball);
                                        aux = J;
                                    }
                                    if(!end && Cont_Indice_Ball < 20){
                                        Bola[Cont_Indice_Ball] = new Ball(ball);
                                        Bola[Cont_Indice_Ball].Iniciar_Ruido();
                                        Cont_Indice_Ball++;
                                    }
                                    else if(Cont_Indice_Ball < 20){
                                        if(!Bola[aux].Ruido_Inicializado()){
                                            Bola[aux].Iniciar_Ruido();
                                        }
                                    }
                                }
                            }
                            // definir as bolas a serem mostradas em tela
                            bool Bola_Unica = false; // Váriavel que vai garantir que apenas 1 bola vai ser mostrada
                            for(int J=0; J < Cont_Indice_Ball; J++){
                                Bola[J].Filtro_Ruido();
                                if(Bola[J].get_Ativo() && Bola[J].get_Valido && !Bola_Unica){// Se a bola estiver Ativa e já tiver sido validada, ela começa a ser mostrada em tela e ter o filtro de perda ativo
                                    Bola_Unica = true;
                                    std::thread T1(Bola[J].kalman());
                                    std::thread T2(Bola[J].Perda());
                                    T1.join();
                                    std::thread T3(Bola[J].printRobotInfo());

                                }
                                else if(Bola[J].get_Ativo()){ /*  Sa bola  estiver Ativa porém n tiver sido validada ainda, é verificado se ela foi atualizada nesse momento, se sim,
                                ruido continua e é calculado sua próxima posição pelo filtro de kalman, se não, 
                                o filtro de ruido é parado e essa bola é esquecida até que seja encontrada novamente */

                                    if(Bola[J].get_Atualizado()){

                                        std::thread T1(Bola[J].kalman());
                                        T1.join();
                            
                                    }
                                    else{
                                        Bola[J].SET_OFF_RUIDO();
                                        Bola[J].set_off_Ativo();
                                    }
                                }
                            }
                        }
                        #pragma omp section{
                            //Blue robot info:

                            //loop de verificações e manipulação robos azuis
                            for (int i = 0; i < robots_blue_n; i++) {
                                int aux;
                                SSL_DetectionRobot robot = detection.robots_blue(i);
                                if (robot.has_robot_id()) {
                                    //Se for novo ID
                                    if(robot.robot_id >= Cont_Indice_blue ){
                                        Blue[Cont_Indice_blue] = new Robos(robot);
                                        Blue[Cont_Indice_blue].Iniciar_Ruido();
                                        Cont_Indice_blue++;
                                    }
                                    else{ // Verificando a qual robo pertence o ID
                                        bool end = false;
                                        for(int J=0; J < Cont_Indice_blue && !end; J++){ // Se o ID já existe e se pertence ao intervalo de robos
                                            end = Blue[J].Verificar(robot);
                                            aux = J;
                                        }
                                        if(!end && Cont_Indice_blue < 20){ // caso o ID existe mas seu ID é menor que o Cont e n está na lista
                                            Blue[Cont_Indice_blue] = new Robos(robot);
                                            Blue[Cont_Indice_blue].Iniciar_Ruido();
                                            Cont_Indice_blue++;
                                        }
                                        else if(Cont_Indice_blue < 20) {// Verificar se devo manter ou resetar o contador do ruido
                                            if(!Blue[aux].Ruido_Inicializado()){
                                                Blue[aux].Iniciar_Ruido();
                                            }
                                        }
                                    }
                                }
                            }
                            
                            // definir robos a serem mostrados em tela
                            for(int J=0; J < Cont_Indice_blue; J++){
                                Blue[J].Filtro_Ruido();
                                if(Blue[J].get_Ativo() && Blue[J].get_Valido){// Se o robo estiver Ativo e já tiver sido validado, ele começa a ser mostrado em tela e ter o filtro de perda ativo
                                    std::thread T1(Blue[J].kalman());
                                    std::thread T2(Blue[J].Perda());
                                    T1.join();
                                    std::thread T3(Blue[J].printRobotInfo());
                                    
                                    if(Blue[J].getx() <= 0){
                                        grSim_client.sendCommand(1.0, J);
                                    }else{
                                        grSim_client.sendCommand(-1.0, J);
                                    }

                                }
                                else if(Blue[J].get_Ativo()){ /*  Se o robo estiver Ativo porém n tiver sido validado ainda, é verificado se ele foi atualizado nesse momento, se sim,
                                ruido continua e é calculado sua próxima posição pelo filtro de kalman, se não, 
                                o filtro de ruido é parado e esse Robo é esquecido até que seja encontrado novamente */

                                    if(Blue[J].get_Atualizado()){

                                        std::thread T1(Blue[J].kalman());
                                        T1.join();
                            
                                    }
                                    else{
                                        Blue[J].SET_OFF_RUIDO();
                                        Blue[J].set_off_Ativo();
                                    }
                                }
                            }
                            //....................................................
                        }
                        #pragma omp section{
                            //Yellow robot info:

                            //loop de verificações e manipulação robos amarelos
                            for (int i = 0; i < robots_yellow_n; i++) {
                                int aux;
                                SSL_DetectionRobot robot = detection.robots_yellow(i);
                                if (robot.has_robot_id()) {
                                    //Se for novo ID
                                    if(robot.robot_id >= Cont_Indice_yellow ){
                                        Yellow[Cont_Indice_yellow] = new Robos(robot);
                                        Yellow[Cont_Indice_yellow].Iniciar_Ruido();
                                        Cont_Indice_yellow++;
                                    }
                                    else{ // Verificando a qual robo pertence o ID
                                        bool end = false;
                                        for(int J=0; J < Cont_Indice_yellow && !end; J++){ // Se o ID já existe e se pertence ao intervalo de robos
                                            end = Yellow[J].Verificar(robot);
                                            aux = J;
                                        }
                                        if(!end && Cont_Indice_yellow < 20 ){ // caso o ID existe mas seu ID é menor que o Cont e n está na lista
                                            Yellow[Cont_Indice_yellow] = new Robos(robot);
                                            Yellow[Cont_Indice_yellow].Iniciar_Ruido();
                                            Cont_Indice_yellow++;
                                        }
                                        else if(Cont_Indice_yellow < 20){// Verificar se devo manter ou resetar o contador do ruido
                                            if(!Yellow[aux].Ruido_Inicializado()){
                                                Yellow[aux].Iniciar_Ruido();
                                            }
                                        }
                                    }
                                    
                                }

                                    printf("-Robot(B) (%2d/%2d): ",i+1, robots_Yellow_n);

                            }
                            
                            // definir robos a serem mostrados em tela
                            for(int J=0; J < Cont_Indice_yellow; J++){
                                Yellow[J].Filtro_Ruido();
                                if(Yellow[J].get_Ativo() && Yellow[J].get_Valido){// Se o robo estiver Ativo e já tiver sido validado, ele começa a ser mostrado em tela e ter o filtro de perda ativo
                                    std::thread T1(Yellow[J].kalman());
                                    std::thread T2(Yellow[J].Perda());
                                    T1.join();
                                    std::thread T3(Yellow[J].printRobotInfo());
                                    
                                    if(Yellow[J].getx() <= 0){
                                        grSim_client.sendCommand(1.0, J);
                                    }else{
                                        grSim_client.sendCommand(-1.0, J);
                                    }

                                }
                                else if(Yellow[J].get_Ativo()){ /*  Se o robo estiver Ativo porém n tiver sido validado ainda, é verificado se ele foi atualizado nesse momento, se sim,
                                ruido continua e é calculado sua próxima posição pelo filtro de kalman, se não, 
                                o filtro de ruido é parado e esse Robo é esquecido até que seja encontrado novamente */

                                    if(Yellow[J].get_Atualizado()){

                                        Yellow[J].kalman();
                                        
                            
                                    }
                                    else{
                                        Yellow[J].SET_OFF_RUIDO();
                                        Yellow[J].set_off_Ativo();
                                    }
                                } 
                            }
                            //....................................................
                        }
                    }
                }
            }
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