/*
 * Copyright (C) 2018 dimercur
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "tasks.h"
#include <stdexcept>
#include <valarray>

// Déclaration des priorités des taches
#define PRIORITY_TSERVER 99
#define PRIORITY_TOPENCOMROBOT 50
#define PRIORITY_TMOVE 35
#define PRIORITY_TSENDTOMON 70
#define PRIORITY_TRECEIVEFROMMON 70
#define PRIORITY_TSTARTROBOT 50
#define PRIORITY_TBATTERY 10
#define PRIORITY_TCAMERA 30
#define PRIORITY_TIMAGE 15
#define PRIORITY_RELOAD 40

/*
 * Some remarks:
 * 1- This program is mostly a template. It shows you how to create tasks, semaphore
 *   message queues, mutex ... and how to use them
 * 
 * 2- semDumber is, as name say, useless. Its goal is only to show you how to use semaphore
 * 
 * 3- Data flow is probably not optimal
 * 
 * 4- Take into account that ComRobot::Write will block your task when serial buffer is full,
 *   time for internal buffer to flush
 * 
 * 5- Same behavior existe for ComMonitor::Write !
 * 
 * 6- When you want to write something in terminal, use cout and terminate with endl and flush
 * 
 * 7- Good luck !
 */

/**
 * @brief Initialisation des structures de l'application (tâches, mutex, 
 * semaphore, etc.)
 */
void Tasks::Init() {
    /**************************************************************************************/
    /* 	Mutex creation                                                                    */
    /**************************************************************************************/
    CreateMutex(&mutex_monitor);
    CreateMutex(&mutex_robot);
    CreateMutex(&mutex_robotStarted);
    CreateMutex(&mutex_move);
    CreateMutex(&mutex_camera);
    CreateMutex(&mutex_isImagePeriodic);
    CreateMutex(&mutex_imageMode);
    CreateMutex(&mutex_lostMessagesCnt);
    cout << "Mutexes created successfully" << endl << flush;

    /**************************************************************************************/
    /* 	Semaphors creation       							  */
    /**************************************************************************************/
    CreateSemaphore(&sem_barrier);
    CreateSemaphore(&sem_openComRobot);
    CreateSemaphore(&sem_serverOk);
    CreateSemaphore(&sem_startRobot);
    CreateSemaphore(&sem_startCamera);
    CreateSemaphore(&sem_closeCamera);
    cout << "Semaphores created successfully" << endl << flush;

    /**************************************************************************************/
    /* Tasks creation                                                                     */
    /**************************************************************************************/
    CreateTask(&th_server, "th_server", PRIORITY_TSERVER);
    CreateTask(&th_sendToMon, "th_sendToMon", PRIORITY_TSENDTOMON);
    CreateTask(&th_receiveFromMon, "th_receiveFromMon", PRIORITY_TRECEIVEFROMMON);
    CreateTask(&th_openComRobot, "th_openComRobot", PRIORITY_TOPENCOMROBOT);
    CreateTask(&th_startRobot, "th_startRobot", PRIORITY_TSTARTROBOT);
    CreateTask(&th_move, "th_move", PRIORITY_TMOVE);
    CreateTask(&th_battery, "th_battery", PRIORITY_TBATTERY);
    CreateTask(&th_camera, "th_camera", PRIORITY_TCAMERA);
    CreateTask(&th_image, "th_image", PRIORITY_TIMAGE);
    CreateTask(&th_reload, "th_reload", PRIORITY_RELOAD);
    cout << "Tasks created successfully" << endl << flush;

    /**************************************************************************************/
    /* Message queues creation                                                            */
    /**************************************************************************************/
    CreateQueue(&q_messageToMon, "q_messageToMon");
    cout << "Queues created successfully" << endl << flush;

}

/**
 * @brief Démarrage des tâches
 */
void Tasks::Run() {
    rt_task_set_priority(NULL, T_LOPRIO);

    RunTask(&th_server, (void(*)(void*)) & Tasks::ServerTask);
    RunTask(&th_sendToMon, (void(*)(void*)) & Tasks::SendToMonTask);
    RunTask(&th_receiveFromMon, (void(*)(void*)) & Tasks::ReceiveFromMonTask);
    RunTask(&th_openComRobot, (void(*)(void*)) & Tasks::OpenComRobot);
    RunTask(&th_startRobot, (void(*)(void*)) & Tasks::StartRobotTask);
    RunTask(&th_move, (void(*)(void*)) & Tasks::MoveTask);
    RunTask(&th_battery, (void(*)(void*)) & Tasks::BatteryTask);
    RunTask(&th_camera, (void(*)(void*)) & Tasks::CameraTask);
    RunTask(&th_image, (void(*)(void*)) & Tasks::ImageTask);

    cout << "Tasks launched" << endl << flush;
}

/**
 * @brief Arrêt des tâches
 */
void Tasks::Stop() {
    monitor.Close();
    robot.Close();
}

/**
 */
void Tasks::Join() {
    cout << "Tasks synchronized" << endl << flush;
    rt_sem_broadcast(&sem_barrier);
    pause();
}

/**
 * @brief Thread handling server communication with the monitor.
 */
void Tasks::ServerTask(void *arg) {
    int status;

    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are started)
    rt_sem_p(&sem_barrier, TM_INFINITE);

    /**************************************************************************************/
    /* The task server starts here                                                        */
    /**************************************************************************************/
    rt_mutex_acquire(&mutex_monitor, TM_INFINITE);
    status = monitor.Open(SERVER_PORT);
    rt_mutex_release(&mutex_monitor);

    cout << "Open server on port " << (SERVER_PORT) << " (" << status << ")" << endl;

    if (status < 0) throw std::runtime_error {
        "Unable to start server on port " + std::to_string(SERVER_PORT)
    };
    monitor.AcceptClient(); // Wait the monitor client
    cout << "Rock'n'Roll baby, client accepted!" << endl << flush;
    rt_sem_broadcast(&sem_serverOk);
}

/**
 * @brief Thread sending data to monitor.
 */
void Tasks::SendToMonTask(void* arg) {
    Message *msg;

    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);

    /**************************************************************************************/
    /* The task sendToMon starts here                                                     */
    /**************************************************************************************/
    rt_sem_p(&sem_serverOk, TM_INFINITE);

    while (1) {
        cout << "wait msg to send" << endl << flush;
        msg = ReadInQueue(&q_messageToMon);
        cout << "Send msg to mon: " << msg->ToString() << endl << flush;
        rt_mutex_acquire(&mutex_monitor, TM_INFINITE);
        monitor.Write(msg); // The message is deleted with the Write
        rt_mutex_release(&mutex_monitor);
    }
}

/**
 * @brief Thread receiving data from monitor.
 */
void Tasks::ReceiveFromMonTask(void *arg) {
    Message *msgRcv;

    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);

    /**************************************************************************************/
    /* The task receiveFromMon starts here                                                */
    /**************************************************************************************/
    rt_sem_p(&sem_serverOk, TM_INFINITE);
    cout << "Received message from monitor activated" << endl << flush;

    while (1) {
        msgRcv = monitor.Read();
        cout << "Rcv <= " << msgRcv->ToString() << endl << flush;

        if (msgRcv->CompareID(MESSAGE_MONITOR_LOST)) {
            delete(msgRcv);
            exit(-1);
        } else if (msgRcv->CompareID(MESSAGE_ROBOT_COM_OPEN)) {
            rt_sem_v(&sem_openComRobot);
        } else if (msgRcv->CompareID(MESSAGE_ROBOT_START_WITHOUT_WD)) {
            this->startMode = START_WITHOUT_WD;
            rt_sem_v(&sem_startRobot);
        } else if (msgRcv->CompareID(MESSAGE_ROBOT_START_WITH_WD)) {
            this->startMode = START_WITH_WD;
            rt_sem_v(&sem_startRobot);
        } else if (msgRcv->CompareID(MESSAGE_ROBOT_GO_FORWARD) ||
                msgRcv->CompareID(MESSAGE_ROBOT_GO_BACKWARD) ||
                msgRcv->CompareID(MESSAGE_ROBOT_GO_LEFT) ||
                msgRcv->CompareID(MESSAGE_ROBOT_GO_RIGHT) ||
                msgRcv->CompareID(MESSAGE_ROBOT_STOP)) {

            
            rt_mutex_acquire(&mutex_move, TM_INFINITE);
            move = msgRcv->GetID();
            rt_mutex_release(&mutex_move);
        } else if (msgRcv->CompareID(MESSAGE_CAM_OPEN)) {
            cout << "[ReceiveFromMon] Received MESSAGE_CAM_OPEN" << endl;
            rt_sem_v(&sem_startCamera);
        } else if (msgRcv->CompareID(MESSAGE_CAM_CLOSE)) {
            rt_sem_v(&sem_closeCamera);
        }
        delete(msgRcv); // mus be deleted manually, no consumer
    }
}

/**
 * @brief Thread opening communication with the robot.
 */
void Tasks::OpenComRobot(void *arg) {
    int status;
    int err;

    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);

    /**************************************************************************************/
    /* The task openComRobot starts here                                                  */
    /**************************************************************************************/
    while (1) {
        rt_sem_p(&sem_openComRobot, TM_INFINITE);
        cout << "Open serial com (";
        rt_mutex_acquire(&mutex_robot, TM_INFINITE);
        status = robot.Open();
        rt_mutex_release(&mutex_robot);
        cout << status;
        cout << ")" << endl << flush;

        Message * msgSend;
        if (status < 0) {
            msgSend = new Message(MESSAGE_ANSWER_NACK);
        } else {
            msgSend = new Message(MESSAGE_ANSWER_ACK);
        }
        WriteInQueue(&q_messageToMon, msgSend); // msgSend will be deleted by sendToMon
    }
}

/**
 * @brief Thread starting the communication with the robot.
 */
void Tasks::StartRobotTask(void *arg) {
    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);

    /**************************************************************************************/
    /* The task startRobot starts here                                                    */
    /**************************************************************************************/
    while (1) {
        Message * msgSend;
        rt_sem_p(&sem_startRobot, TM_INFINITE);

        // Robot start mode.
        int mode = this->startMode;

        // If it is an unknown mode.
        if (mode != START_WITHOUT_WD && mode != START_WITH_WD) {
            ostringstream oss;
            oss << "Unknown start mode " << mode;
            throw std::runtime_error{oss.str()};
        }

        if (mode == START_WITHOUT_WD) {
            cout << "Start robot without watchdog (";
        } else {
            cout << "Start robot with watchdog (";
        }

        if (mode == START_WITHOUT_WD) {
            msgSend = WriteToRobot(robot.StartWithoutWD());
        } else {
            msgSend = WriteToRobot(robot.StartWithWD());
        }

        cout << msgSend->GetID();
        cout << ")" << endl;

        cout << "Movement answer: " << msgSend->ToString() << endl << flush;
        WriteInQueue(&q_messageToMon, msgSend); // msgSend will be deleted by sendToMon

        if (msgSend->GetID() == MESSAGE_ANSWER_ACK) {
            rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
            robotStarted = 1;
            rt_mutex_release(&mutex_robotStarted);

            // Start the reload task.
            RunTask(&th_reload, (void(*)(void*)) & Tasks::ReloadTask);
        }
    }
}

/**
 * @brief Thread handling control of the robot.
 */
void Tasks::MoveTask(void *arg) {
    int rs;
    int cpMove;

    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);

    /**************************************************************************************/
    /* The task starts here                                                               */
    /**************************************************************************************/
    rt_task_set_periodic(NULL, TM_NOW, 100000000);

    while (1) {
        rt_task_wait_period(NULL);
        cout << "Periodic movement update";
        rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
        rs = robotStarted;
        rt_mutex_release(&mutex_robotStarted);
        if (rs == 1) {
            rt_mutex_acquire(&mutex_move, TM_INFINITE);
            cpMove = move;
            rt_mutex_release(&mutex_move);

            cout << " move: " << cpMove;

            WriteToRobot((MessageID) cpMove) ;
        }
        cout << endl << flush;
    }
}

/**
 * @brief Thread updating the monitor with the robot battery level.
 */
void Tasks::BatteryTask(void *arg) {
    int rs;

    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);

    /**************************************************************************************/
    /* The task starts here                                                               */
    /**************************************************************************************/
    rt_task_set_periodic(NULL, TM_NOW, 500000000); // 500ms = 5e8 ns

    while (1) {
        rt_task_wait_period(NULL);

        // Get the robot starting status.
        rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
        rs = robotStarted;
        rt_mutex_release(&mutex_robotStarted);

        // Waiting for robot started.
        if (rs == 1) {
            Message * response = WriteToRobot(MESSAGE_ROBOT_BATTERY_GET) ;

            if (response->CompareID(MESSAGE_ROBOT_BATTERY_LEVEL)) {
                WriteInQueue(&q_messageToMon, response);
            }
        }
    }
}

/**
 * @brief Thread opening and closing the camera.
 */
void Tasks::CameraTask(void *arg) {
    Message message;

    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);

    bool cameraOpened = false;

    /**************************************************************************************/
    /* The task starts here                                                    */
    /**************************************************************************************/
    while (1) {
        while (not cameraOpened) {

            rt_sem_p(&sem_startCamera, TM_INFINITE);

            rt_mutex_acquire(&mutex_camera, TM_INFINITE);
            bool cameraOpened = camera.Open();
            rt_mutex_release(&mutex_camera);

            /*if (cameraOpened) {
                message = Message(MESSAGE_ANSWER_ACK);
            } else {
                message = Message(MESSAGE_ANSWER_NACK);
            }
            WriteInQueue(&q_messageToMon, &message);*/
        }
        while (cameraOpened) {
            
            rt_sem_p(&sem_closeCamera, TM_INFINITE);

            rt_mutex_acquire(&mutex_camera, TM_INFINITE);
            camera.Close();
            bool cameraOpened = camera.IsOpen();
            rt_mutex_release(&mutex_camera);

            if (not cameraOpened) {
                message = Message(MESSAGE_ANSWER_ACK);
            } else {
                message = Message(MESSAGE_ANSWER_NACK);
            }
            WriteInQueue(&q_messageToMon, &message);
        }
    }
}

/**
 * @brief Thread sending an image to the monitor.
 */
void Tasks::ImageTask(void *arg) {
    int localIsImagePeriodic;
    int localImageMode;
    
    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);
    
    /**************************************************************************************/
    /* The task starts here                                                               */
    /**************************************************************************************/

    rt_task_set_periodic(NULL, TM_NOW, 100000000); // 100ms = 1e8 ns

    while (1) {
        rt_task_wait_period(NULL);        
        
        rt_mutex_acquire(&mutex_isImagePeriodic, TM_INFINITE);
        localIsImagePeriodic = isImagePeriodic;
        rt_mutex_release(&mutex_isImagePeriodic);

        if (localIsImagePeriodic) {
            rt_mutex_acquire(&mutex_imageMode, TM_INFINITE);
            localImageMode = imageMode;
            rt_mutex_release(&mutex_imageMode);

            if (localImageMode == IMAGEMODE_IMG) {
              
              rt_mutex_acquire(&mutex_camera, TM_INFINITE); 
              
              if(camera.IsOpen()){
              cout << "Image Grabed1" << endl << flush;
              Img image = camera.Grab();
              cout << "Image Grabed2" << endl << flush;
              monitor.Write(new MessageImg(MESSAGE_CAM_IMAGE, &image));
              //WriteInQueue(&q_messageToMon, new MessageImg(MESSAGE_CAM_IMAGE, &image));
              }
              rt_mutex_release(&mutex_camera);
             
              
                
            }
        }
    }
}

/**
 * @brief Thread sending periodically watchdog reload to the robot.
 * @warning This thread should be started after barrier synchronization. 
 * The robot should be started.
 */
void Tasks::ReloadTask(void * arg) {
    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;

    rt_task_set_periodic(NULL, TM_NOW, 1000000000); // 1000ms = 1e9 ns

    int rs;
    while (1) {
        rt_task_wait_period(NULL);

        // Get the robot starting status.
        rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
        rs = robotStarted;
        rt_mutex_release(&mutex_robotStarted);

        // Waiting for robot started and a WD start.
        if ((rs == 1) && (this->startMode == START_WITH_WD)) {
            WriteToRobot(MESSAGE_ROBOT_RELOAD_WD) ;
            cout << "Reload message sent" << endl << flush ;
        }
    }
}

/**
 * @brief Write a message to the robot.
 */
Message * Tasks::WriteToRobot(MessageID messageId) {
    return this->WriteToRobot(new Message(messageId)) ;
}


/**
 * @brief Write a message to the robot.
 */
Message * Tasks::WriteToRobot(Message * message) {
    // Send the message to the robot.
    rt_mutex_acquire(&mutex_robot, TM_INFINITE);
    Message* response = this->robot.Write(message);
    rt_mutex_release(&mutex_robot);
    
    // Check the error counter.
    rt_mutex_acquire(&mutex_lostMessagesCnt, TM_INFINITE);
    
    if(response->CompareID(MESSAGE_ANSWER_ROBOT_TIMEOUT)) {
        this->lostMessagesCnt++;
        
        // The connection was lost.
        if(this->lostMessagesCnt >= MAX_LOST_MESSAGES) {
            cout << "[ERROR] Communication LOST" << endl ;
            
            // Send a lost message to the monitor.
            WriteInQueue(&q_messageToMon, new Message(MESSAGE_ANSWER_ROBOT_TIMEOUT)) ;
            
            // Stop the robot for all the threads.
            rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
            this->robotStarted = 0 ;
            rt_mutex_release(&mutex_robotStarted);
            
            // Close the communication with the robot.
            Tasks::CloseRobotCommunication() ;
            
            // Reopen the communication.
            rt_sem_v(&sem_openComRobot) ;
        }
    } else {
        this->lostMessagesCnt = 0 ;
    }
    
    rt_mutex_release(&mutex_lostMessagesCnt) ;
    
    return response ;
}

/**
 * @brief Close the communication with the robot.
 */
void Tasks::CloseRobotCommunication() {
    rt_mutex_acquire(&mutex_robot, TM_INFINITE) ;
    int closeStatus = this->robot.Close() ;
    rt_mutex_release(&mutex_robot) ;

    if(closeStatus != 0) {
        cout << "[ERROR] Close robot failed" << endl ;
    }
}

/**
 * Write a message in a given queue
 * @param queue Queue identifier
 * @param msg Message to be stored
 */
void Tasks::WriteInQueue(RT_QUEUE *queue, Message *msg) {
    int err;
    if ((err = rt_queue_write(queue, (const void *) &msg, sizeof ((const void *) &msg), Q_NORMAL)) < 0) {
        cerr << "Write in queue failed: " << strerror(-err) << endl << flush;
        throw std::runtime_error{"Error in write in queue"};
    }
}

/**
 * Read a message from a given queue, block if empty
 * @param queue Queue identifier
 * @return Message read
 */
Message *Tasks::ReadInQueue(RT_QUEUE *queue) {
    int err;
    Message *msg;

    if ((err = rt_queue_read(queue, &msg, sizeof ((void*) &msg), TM_INFINITE)) < 0) {
        cout << "Read in queue failed: " << strerror(-err) << endl << flush;
        throw std::runtime_error{"Error in read in queue"};
    }/** else {
        cout << "@msg :" << msg << endl << flush;
    } /**/

    return msg;
}

void Tasks::CreateMutex(RT_MUTEX *mutex) {
    if (int err = rt_mutex_create(mutex, NULL)) {
        cerr << "Error mutex create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
}

void Tasks::CreateSemaphore(RT_SEM *sem) {
    if (int err = rt_sem_create(sem, NULL, 0, S_FIFO)) {
        cerr << "Error semaphore create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
}

void Tasks::CreateTask(RT_TASK *task, const char *name, int priority) {
    if (int err = rt_task_create(task, name, 0, priority, 0)) {
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
}

void Tasks::CreateQueue(RT_QUEUE *queue, const char *name) {
    if (int err = rt_queue_create(queue, name, sizeof (Message*)*50, Q_UNLIMITED, Q_FIFO)) {
        cerr << "Error msg queue create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
}

void Tasks::RunTask(RT_TASK *task, void(* entry)(void *)) {
    if (int err = rt_task_start(task, entry, this)) {
        cerr << "Error task start: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
}