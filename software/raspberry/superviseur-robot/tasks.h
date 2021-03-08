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

#ifndef __TASKS_H__
#define __TASKS_H__

#include <unistd.h>
#include <iostream>
#include <sstream>

#include <sys/mman.h>
#include <alchemy/task.h>
#include <alchemy/timer.h>
#include <alchemy/mutex.h>
#include <alchemy/sem.h>
#include <alchemy/queue.h>

#include "messages.h"
#include "commonitor.h"
#include "comrobot.h"
#include "camera.h"
#include "img.h"

#define CAMERA_SIZE sm
#define CAMERA_FPS 20

// Start mode values.
#define START_WITHOUT_WD 0
#define START_WITH_WD    1

/**
 * Enumerate for image mode
 */
enum e_imageMode {IMAGEMODE_IMG, IMAGEMODE_IMG_POS};

using namespace std;

class Tasks {
public:
    /**
     * @brief Initializes main structures (semaphores, tasks, mutex, etc.)
     */
    void Init();

    /**
     * @brief Starts tasks
     */
    void Run();

    /**
     * @brief Stops tasks
     */
    void Stop();
    
    /**
     * @brief Suspends main thread
     */
    void Join();
    
private:
    /**********************************************************************/
    /* Shared data                                                        */
    /**********************************************************************/
    ComMonitor monitor;
    ComRobot robot;
    Camera camera{CAMERA_SIZE, CAMERA_FPS};
    int robotStarted = 0;
    int move = MESSAGE_ROBOT_STOP;
    int isImagePeriodic = 0;
    int imageMode = IMAGEMODE_IMG;
    
    /**
     * Start mode value.
     * 0 : without watchdog
     * 1 : with watchdog.
     */
    int startMode ;
    int compteurMsgError=0;
    
    /**********************************************************************/
    /* Tasks                                                              */
    /**********************************************************************/
    RT_TASK th_server;
    RT_TASK th_sendToMon;
    RT_TASK th_receiveFromMon;
    RT_TASK th_openComRobot;
    RT_TASK th_startRobot;
    RT_TASK th_move;
    RT_TASK th_battery;
    RT_TASK th_camera;
    RT_TASK th_image;
    RT_TASK th_reload;
    
    /**********************************************************************/
    /* Mutex                                                              */
    /**********************************************************************/
    RT_MUTEX mutex_monitor;
    RT_MUTEX mutex_robot;
    RT_MUTEX mutex_robotStarted;
    RT_MUTEX mutex_move;
    RT_MUTEX mutex_camera;
    RT_MUTEX mutex_isImagePeriodic;
    RT_MUTEX mutex_imageMode;

    /**********************************************************************/
    /* Semaphores                                                         */
    /**********************************************************************/
    RT_SEM sem_barrier;
    RT_SEM sem_openComRobot;
    RT_SEM sem_serverOk;
    RT_SEM sem_startRobot;
    RT_SEM sem_startCamera;
    RT_SEM sem_closeCamera;

    /**********************************************************************/
    /* Message queues                                                     */
    /**********************************************************************/
    int MSG_QUEUE_SIZE;
    RT_QUEUE q_messageToMon;
    
    /**********************************************************************/
    /* Tasks' functions                                                   */
    /**********************************************************************/
    
    /**
     * @brief Helper method, exits on error
     */
    static void CreateMutex(RT_MUTEX *mutex);
    /**
     * @brief Helper method, exits on error
     */
    static void CreateSemaphore(RT_SEM *sem);
    /**
     * @brief Helper method, exits on error
     */
    static void CreateTask(RT_TASK *task, const char *name, int priority);
    /**
     * @brief Helper method, exits on error
     */
    static void CreateQueue(RT_QUEUE *queue, const char *name);
    /**
     * @brief Helper method, exits on error
     */
    void RunTask(RT_TASK *task, void(* entry)(void *));
    
    /**
     * @brief Thread handling server communication with the monitor.
     */
    void ServerTask(void *arg);
     
    /**
     * @brief Thread sending data to monitor.
     */
    void SendToMonTask(void *arg);
        
    /**
     * @brief Thread receiving data from monitor.
     */
    void ReceiveFromMonTask(void *arg);
    
    /**
     * @brief Thread opening communication with the robot.
     */
    void OpenComRobot(void *arg);

    /**
     * @brief Thread starting the communication with the robot.
     */
    void StartRobotTask(void *arg);
    
    /**
     * @brief Thread handling control of the robot.
     */
    void MoveTask(void *arg);
    
     /**
     * @brief Thread updating the monitor with the robot battery level.
     */
    void BatteryTask(void *arg);
    
     /**
     * @brief Thread opening and closing the camera.
     */
    void CameraTask(void *arg);
    
     /**
     * @brief Thread sending an image to the monitor.
     */
    void ImageTask(void *arg);

     /**
     * @brief Thread sending an image to the monitor.
     */
    void ReloadTask(void *arg);
    
    /**********************************************************************/
    /* Queue services                                                     */
    /**********************************************************************/
    /**
     * Write a message in a given queue
     * @param queue Queue identifier
     * @param msg Message to be stored
     */
    void WriteInQueue(RT_QUEUE *queue, Message *msg);
    
    /**
     * Read a message from a given queue, block if empty
     * @param queue Queue identifier
     * @return Message read
     */
    Message *ReadInQueue(RT_QUEUE *queue);

};

#endif // __TASKS_H__ 

