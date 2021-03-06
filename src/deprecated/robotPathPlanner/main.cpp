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

/**
 * \section robotPathPlanner
 * This module performs global path-planning by generating a sequence of waypoints to be tracked by a local navigation algorithm.
 *  It receives a goal from the user either via RPC command or via yarp iNavigation2D interface and it computes a sequence of waypoints which are sent one by one to a local navigator module.
 * If the local navigation module fails to reach one of these waypoints, the global navigation is aborted too. 
 * A detailed description of configuration parameters available for the module is provided in the README.md file.
 */

#include <yarp/os/Network.h>
#include <yarp/os/RFModule.h>
#include <yarp/os/Time.h>
#include <yarp/os/Port.h>
#include <yarp/dev/ControlBoardInterfaces.h>

#include "pathPlanner.h"
#include <math.h>

class robotPlannerModule : public yarp::os::RFModule
{
protected:
    PlannerThread     *plannerThread;
    yarp::os::Port     rpcPort;

public:
    robotPlannerModule()
    {
        plannerThread=NULL;
    }

    virtual bool configure(yarp::os::ResourceFinder &rf)
    {
        plannerThread = new PlannerThread(0.020,rf);

        bool ret = rpcPort.open("/robotPathPlanner/rpc");
        if (ret == false)
        {
            yError() << "Unable to open module ports";
            return false;
        }
        attach(rpcPort);
        //attachTerminal();

        if (!plannerThread->start())
        {
            delete plannerThread;
            return false;
        }

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
        plannerThread->stop();
        delete plannerThread;
        plannerThread=NULL;

        return true;
    }

    virtual double getPeriod()
    { 
        return 3.0; 
    }
    
    virtual bool updateModule()
    { 
        if (isStopping())
        {
            plannerThread->stop();
            return false;
        }
        
        int loc, las, sta;
        plannerThread->getTimeouts(loc,las,sta);

        bool err = false;
        if (las>TIMEOUT_MAX)
        {
            yError("timeout, no laser data received!\n");
            err= true;
        }
        if (loc>TIMEOUT_MAX)
        {
            yError(" timeout, no localization data received!\n");
            err= true;
        }
        if (sta>TIMEOUT_MAX)
        {
            yError("timeout, no status info received!\n");
            err= true;
        }
        
        if (err==false)
            yInfo() << "module running, ALL ok. Navigation status:" << plannerThread->getNavigationStatusAsString();

        return true; 
    }

    bool parse_respond_string(const yarp::os::Bottle& command, yarp::os::Bottle& reply)
    {
        if (command.get(0).isString() && command.get(0).asString() == "gotoAbs")
        {
            yarp::dev::Map2DLocation loc;
            loc.x = command.get(1).asDouble();
            loc.y = command.get(2).asDouble();
            if   (command.size() == 4) { loc.theta = command.get(3).asDouble(); }
            else                       { loc.theta = nan(""); }
            loc.map_id = plannerThread->getCurrentMapId();
            plannerThread->setNewAbsTarget(loc);
            reply.addString("new absolute target received");
        }

        else if (command.get(0).isString() && command.get(0).asString() == "gotoRel")
        {
            yarp::sig::Vector v;
            v.push_back(command.get(1).asDouble());
            v.push_back(command.get(2).asDouble());
            if (command.size() == 4) { v.push_back(command.get(3).asDouble()); }
            else                     { v.push_back(nan("")); }
            plannerThread->setNewRelTarget(v);
            reply.addString("new relative target received");
        }

        else if (command.get(0).isString() && command.get(0).asString() == "goto")
        {
            std::string location_name = command.get(1).asString();
            if (plannerThread->gotoLocation(location_name))
            {
                reply.addString("goto done");
            }
            else
            {
                reply.addString("goto error");
            }
        }

        else if (command.get(0).isString() && command.get(0).asString() == "store_current_location")
        {
            std::string location_name = command.get(1).asString();
            plannerThread->storeCurrentLocation(location_name);
            reply.addString("store_current_location done");
        }

        else if (command.get(0).isString() && command.get(0).asString() == "delete_location")
        {
            std::string location_name = command.get(1).asString();
            plannerThread->deleteLocation(location_name);
            reply.addString("delete_location done");
        }

        else if (command.get(0).isString() && command.get(0).asString() == "get_last_target")
        {
            string last_target;
            bool b = plannerThread->getLastTarget(last_target);
            if (b)
            {
                reply.addString(last_target);
            }
            else
            {
                yError() << "get_last_target failed: goto <location_name> target not found.";
                reply.addString("not found");
            }
        }

        else if (command.get(0).asString() == "get")
        {
            if (command.get(1).asString() == "navigation_status")
            {
                string s = plannerThread->getNavigationStatusAsString();
                reply.addString(s.c_str());
            }
        }
        else if (command.get(0).isString() && command.get(0).asString() == "stop")
           
        {
            plannerThread->stopMovement();
            reply.addString("Stopping movement.");
        }
        else if (command.get(0).isString() && command.get(0).asString() == "pause")
        {
            double time = -1;
            if (command.size() > 1)
                time = command.get(1).asDouble();
            plannerThread->pauseMovement(time);
            reply.addString("Pausing.");
        }
        else if (command.get(0).isString() && command.get(0).asString() == "resume")
        {
            plannerThread->resumeMovement();
            reply.addString("Resuming.");
        }
        else if (command.get(0).isString() && command.get(0).asString() == "draw_locations")
        {
            if (command.get(1).asInt() == 1) 
            {
                plannerThread->m_enable_draw_all_locations = true;
                yDebug() << "locations drawing enabled";
            }
            else
            {
                plannerThread->m_enable_draw_all_locations = false;
                yDebug() << "locations drawing disabled";
            }
        }
        else if (command.get(0).isString() && command.get(0).asString() == "draw_enlarged_scans")
        {
            if (command.get(1).asInt() == 1) 
            {
                plannerThread->m_enable_draw_enlarged_scans = true;
                yDebug() << "enlarged scans drawing enabled";
            }
            else
            {
                plannerThread->m_enable_draw_enlarged_scans = false;
                yDebug() << "enlarged scans drawing disabled";
            }
        }
        else
        {
            reply.addString("Unknown command.");
        }
        return true;
    }

    bool parse_respond_vocab(const yarp::os::Bottle& command, yarp::os::Bottle& reply)
    {
        int request = command.get(1).asVocab();
        if (request == VOCAB_NAV_GOTOABS)
        {
            yarp::dev::Map2DLocation loc;
            loc.map_id = command.get(2).asString();
            loc.x = command.get(3).asDouble();
            loc.y = command.get(4).asDouble();
            loc.theta = command.get(5).asDouble();
            plannerThread->setNewAbsTarget(loc);
            reply.addVocab(VOCAB_OK);
        }

        else if (request == VOCAB_NAV_GOTOREL)
        {
            yarp::sig::Vector v;
            v.push_back(command.get(2).asDouble());
            v.push_back(command.get(3).asDouble());
            if (command.size() == 5) v.push_back(command.get(4).asDouble());
            plannerThread->setNewRelTarget(v);
            reply.addVocab(VOCAB_OK);
        }
        else if (request == VOCAB_NAV_GET_STATUS)
        {
            int nav_status = plannerThread->getNavigationStatusAsInt();
            reply.addVocab(VOCAB_OK);
            reply.addInt(nav_status);
        }
        else if (request == VOCAB_NAV_STOP)
        {
            plannerThread->stopMovement();
            reply.addVocab(VOCAB_OK);
        }
        else if (request == VOCAB_NAV_SUSPEND)
        {
            double time = -1;
            if (command.size() > 1)
                time = command.get(1).asDouble();
            plannerThread->pauseMovement(time);
            reply.addVocab(VOCAB_OK);
        }
        else if (request == VOCAB_NAV_RESUME)
        {
            plannerThread->resumeMovement();
            reply.addVocab(VOCAB_OK);
        }
        else if (request == VOCAB_NAV_GET_CURRENT_POS)
        {
            yarp::dev::Map2DLocation loc;
            plannerThread->getCurrentPos(loc);
            reply.addVocab(VOCAB_OK);
            reply.addString(loc.map_id);
            reply.addDouble(loc.x);
            reply.addDouble(loc.y);
            reply.addDouble(loc.theta);

        }
        else if (request == VOCAB_NAV_GET_ABS_TARGET || request == VOCAB_NAV_GET_REL_TARGET)
        {
            Map2DLocation loc;
            if (request == VOCAB_NAV_GET_ABS_TARGET)
            {
                plannerThread->getCurrentAbsTarget(loc);
            }
            else
            {
                plannerThread->getCurrentRelTarget(loc);
            }
            reply.addVocab(VOCAB_OK);

            if(request == VOCAB_NAV_GET_ABS_TARGET) reply.addString(loc.map_id);

            reply.addDouble(loc.x);
            reply.addDouble(loc.y);
            reply.addDouble(loc.theta);
        }
        else
        {
            reply.addVocab(VOCAB_ERR);
        }
        return true;
    }

    virtual bool respond(const yarp::os::Bottle& command,yarp::os::Bottle& reply) 
    {
        reply.clear(); 

        plannerThread->m_mutex.wait();
        if (command.get(0).isVocab())
        {
            if(command.get(0).asVocab() == VOCAB_INAVIGATION && command.get(1).isVocab())
            {
                parse_respond_vocab(command,reply);
            }
            else
            {
                yError() << "Invalid vocab received";
                reply.addVocab(VOCAB_ERR);
            }
        }
        else if (command.get(0).isString())
        {
            if (command.get(0).asString()=="quit")
            {
                plannerThread->m_mutex.post();
                return false;
            }

            else if (command.get(0).asString()=="help")
            {
                reply.addVocab(Vocab::encode("many"));
                reply.addString("Available commands are:");
                reply.addString("goto <locationName>");
                reply.addString("gotoAbs <x> <y> <angle in degrees>");
                reply.addString("gotoRel <x> <y> <angle in degrees>");
                reply.addString("store_current_location <location_name>");
                reply.addString("delete_location <location_name>");
                reply.addString("stop");
                reply.addString("pause");
                reply.addString("resume");
                reply.addString("quit");
                reply.addString("draw_locations <0/1>");
            }
            else if (command.get(0).isString())
            {
                parse_respond_string(command, reply);
            }
        }
        else
        {
            yError() << "Invalid command type";
            reply.addVocab(VOCAB_ERR);
        }
        plannerThread->m_mutex.post();
        return true;
    }
};

int main(int argc, char *argv[])
{
    yarp::os::Network yarp;
    if (!yarp.checkNetwork())
    {
        yError("check Yarp network.\n");
        return -1;
    }

    yarp::os::ResourceFinder rf;
    rf.setVerbose(true);
    rf.setDefaultConfigFile("robotPathPlanner.ini");           //overridden by --from parameter
    rf.setDefaultContext("robotPathPlanner");                  //overridden by --context parameter
    rf.configure(argc,argv);
    std::string debug_rf = rf.toString();

    robotPlannerModule robotPlanner;

    return robotPlanner.runModule(rf);
}

 
