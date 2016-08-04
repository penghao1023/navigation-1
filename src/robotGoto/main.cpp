/* 
 * Copyright (C)2011  Department of Robotics Brain and Cognitive Sciences - Istituto Italiano di Tecnologia
 * Author: Marco Randazzo
 * email:  marco.randazzo@iit.it
 * website: www.robotcub.org
 * Permission is granted to copy, distribute, and/or modify this program
 * under the terms of the GNU General Public License, version 2 or any
 * later version published by the Free Software Foundation.
 *
 * A copy of the license can be found at
 * http://www.robotcub.org/icub/license/gpl.txt
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details
*/

#include <yarp/os/Network.h>
#include <yarp/os/RFModule.h>
#include <yarp/os/Time.h>
#include <yarp/os/Port.h>

#include "robotGoto.h"
#include <math.h>

class robotGotoModule : public yarp::os::RFModule
{
protected:
    GotoThread     *gotoThread;
    yarp::os::Port rpcPort;

public:
    robotGotoModule()
    {
        gotoThread=NULL;
    }

    virtual bool configure(yarp::os::ResourceFinder &rf)
    {
        yarp::os::Time::turboBoost();

        Property p;
        ConstString configFile = rf.findFile("from");
        if (configFile!="") p.fromConfigFile(configFile.c_str());

        gotoThread = new GotoThread(10,rf,p);

        if (!gotoThread->start())
        {
            delete gotoThread;
            return false;
        }

        rpcPort.open("/robotGoto/rpc:i");
        attach(rpcPort);
        //attachTerminal();

        return true;
    }

    virtual bool interruptModule()
    {
        rpcPort.interrupt();

        return true;
    }

    virtual bool close()
    {
        rpcPort.interrupt();
        rpcPort.close();

        //gotoThread->shutdown();
        gotoThread->stop();
        delete gotoThread;
        gotoThread=NULL;

        return true;
    }

    virtual double getPeriod()
    { 
        if (gotoThread) gotoThread->printStats();
        return 1.0; 
    }
    
    virtual bool updateModule()
    { 
        if (isStopping())
        {
            gotoThread->stop();
            return false;
        }
        
        bool err = false;
        if (gotoThread->las_timeout_counter>TIMEOUT_MAX)
        {
            yError (" timeout, no laser data received!");
            err= true;
        }
        if (gotoThread->loc_timeout_counter>TIMEOUT_MAX)
        {
            yError(" timeout, no localization data receive");
            err= true;
        }
        if (gotoThread->odm_timeout_counter>TIMEOUT_MAX)
        {
            yError(" timeout, no odometry data received!");
            err= true;
        }
        
        if (err==false)
        yInfo ("module running, ALL ok");

        return true; 
    }

    virtual bool respond(const yarp::os::Bottle& command,yarp::os::Bottle& reply) 
    {
        reply.clear(); 

        gotoThread->mutex.wait();
        if (command.get(0).asString()=="quit")
        {
            gotoThread->mutex.post();
            return false;
        }

        else if (command.get(0).asString()=="help")
        {
            reply.addVocab(Vocab::encode("many"));
            reply.addString("Available commands are:");
            reply.addString("gotoAbs <x> <y> <angle in degrees>");
            reply.addString("gotoRel <x> <y> <angle in degrees>");
            reply.addString("stop");
            reply.addString("pause");
            reply.addString("resume");
            reply.addString("quit");
            reply.addString("reset_params");
            reply.addString("set linear_tol <m>");
            reply.addString("set linear_ang <deg>");
            reply.addString("set max_lin_speed <m/s>");
            reply.addString("set max_ang_speed <deg/s>");
            reply.addString("set min_lin_speed <m/s>");
            reply.addString("set min_ang_speed <deg/s>");
            reply.addString("set obstacle_stop");
            reply.addString("set obstacle_avoidance");
        }
        else if (command.get(0).isString() && command.get(0).asString() == "reset_params")
        {
            gotoThread->resetParamsToDefaultValue();
        }
        else if ((command.get(0).isString() && command.get(0).asString()=="gotoAbs") ||
                 (command.get(0).isVocab() && command.get(0).asVocab() == VOCAB_NAV_GOTOABS))
        {
            yarp::sig::Vector v;
            v.push_back(command.get(1).asDouble());
            v.push_back(command.get(2).asDouble());
            if (command.size()==4) v.push_back(command.get(3).asDouble());
            gotoThread->setNewAbsTarget(v);
            reply.addString("new absolute target received");
        }

        else if ((command.get(0).isString() && command.get(0).asString() == "gotoRel") ||
                 (command.get(0).isVocab() && command.get(0).asVocab() == VOCAB_NAV_GOTOREL))
        {
            yarp::sig::Vector v;
            v.push_back(command.get(1).asDouble());
            v.push_back(command.get(2).asDouble());
            if (command.size()==4) v.push_back(command.get(3).asDouble());
            gotoThread->setNewRelTarget(v);
            reply.addString("new relative target received");
        }
        else if (command.get(0).asString()=="set")
        {
            if (command.get(1).asString()=="linear_tol")
            {
                gotoThread->m_goal_tolerance_lin = command.get(2).asDouble();
                reply.addString("linear_tol set.");
            }
            else if (command.get(1).asString()=="angular_tol")
            {
                gotoThread->m_goal_tolerance_ang = command.get(2).asDouble();
                reply.addString("angular_tol set.");
            }
            else if (command.get(1).asString()=="max_lin_speed")
            {
                gotoThread->m_max_lin_speed = command.get(2).asDouble();
                reply.addString("max_lin_speed set.");
            }
            else if (command.get(1).asString()=="max_ang_speed")
            {
                gotoThread->m_max_ang_speed = command.get(2).asDouble();
                reply.addString("max_ang_speed set.");
            }
            else if (command.get(1).asString()=="min_lin_speed")
            {
                gotoThread->m_min_lin_speed=command.get(2).asDouble();
                reply.addString("min_lin_speed set.");
            }
            else if (command.get(1).asString()=="min_ang_speed")
            {
                gotoThread->m_min_ang_speed = command.get(2).asDouble();
                reply.addString("min_ang_speed set.");
            }
            else if (command.get(1).asString()=="obstacle_avoidance")
            {
                if (gotoThread->enable_obstacles_avoidance)
                    {
                        reply.addString("enable_obstacles_avoidance=false");
                        gotoThread->enable_obstacles_avoidance=false;
                    }
                else
                    {
                        gotoThread->enable_obstacles_avoidance=true;
                        reply.addString("enable_obstacles_avoidance=true");
                    }
            }
            else if (command.get(1).asString()=="obstacle_stop")
            {
                if (gotoThread->enable_obstacles_avoidance)
                    {
                        reply.addString("enable_obstacle_stop=false");
                        gotoThread->enable_obstacles_emergency_stop=false;
                    }
                else
                    {
                        gotoThread->enable_obstacles_emergency_stop=true;
                        reply.addString("enable_obstacle_stop=true");
                    }
            }
            else
            {
                reply.addString("Unknown set.");
            }
        }
        else if (command.get(0).asString()=="get")
        {
            if (command.get(1).asString()=="navigation_status")
            {
                string s = gotoThread->getNavigationStatus();
                reply.addString(s.c_str());
            }
            else
            {
                reply.addString("Unknown get.");
            }
        }
        else if ((command.get(0).isString() && command.get(0).asString() == "stop") ||
                 (command.get(0).isVocab() && command.get(0).asVocab() == VOCAB_NAV_STOP))
        {
            gotoThread->stopMovement();
            reply.addString("Stopping movement.");
        }
        else if ((command.get(0).isString() && command.get(0).asString() == "pause") ||
                 (command.get(0).isVocab() && command.get(0).asVocab() == VOCAB_NAV_SUSPEND))
        {
            double time = -1;
            if (command.size() > 1)
                time = command.get(1).asDouble();
            gotoThread->pauseMovement(time);
            reply.addString("Pausing.");
        }
        else if ((command.get(0).isString() && command.get(0).asString() == "resume") ||
                 (command.get(0).isVocab() && command.get(0).asVocab() == VOCAB_NAV_RESUME))
        {
            gotoThread->resumeMovement();
            reply.addString("Resuming.");
        }
        else
        {
            reply.addString("Unknown command.");
        }

        gotoThread->mutex.post();
        return true;
    }
};

int main(int argc, char *argv[])
{
    yarp::os::Network yarp;
    if (!yarp.checkNetwork())
    {
        yError("ERROR: check Yarp network.");
        return -1;
    }

    yarp::os::ResourceFinder rf;
    rf.setVerbose(true);
    rf.setDefaultConfigFile("robotGoto.ini");           //overridden by --from parameter
    rf.setDefaultContext("robotGoto");                  //overridden by --context parameter
    rf.configure(argc,argv);
    
    robotGotoModule robotGoto;

    return robotGoto.runModule(rf);
}

 
