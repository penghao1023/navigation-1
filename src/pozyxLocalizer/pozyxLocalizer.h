/*
 * Copyright (C)2018  ICub Facility - Istituto Italiano di Tecnologia
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
#include <yarp/os/Mutex.h>
#include <yarp/os/LockGuard.h>
#include <yarp/os/LogStream.h>
#include <yarp/os/Node.h>
#include <yarp/dev/PolyDriver.h>
#include <yarp/os/Bottle.h>
#include <yarp/sig/Vector.h>
#include <yarp/dev/INavigation2D.h>
#include <yarp/dev/ControlBoardInterfaces.h>
#include <yarp/os/PeriodicThread.h>
#include <math.h>

#ifndef POZYX_LOCALIZER_H
#define POZYX_LOCALIZER_H

using namespace yarp::os;

class pozyxLocalizer;
class pozyxLocalizerThread;

class pozyxLocalizerRPCHandler : public yarp::dev::DeviceResponder
{
protected:
    pozyxLocalizer * interface;
    bool respond(const yarp::os::Bottle& cmd, yarp::os::Bottle& response) override;

public:
    pozyxLocalizerRPCHandler() : interface(NULL) { }
    void setInterface(pozyxLocalizer* iface);
};

class pozyxLocalizer : public yarp::dev::DeviceDriver,
                     public yarp::dev::ILocalization2D
{
public:
    pozyxLocalizerThread*    thread;
    pozyxLocalizerRPCHandler rpcPortHandler;
    yarp::os::Port          rpcPort;

public:
    virtual bool open(yarp::os::Searchable& config) override;

    pozyxLocalizer();
    virtual ~pozyxLocalizer();

    virtual bool close() override;

public:
    /**
    * Gets the current status of the localization task.
    * @return true/false
    */
    bool   getLocalizationStatus(yarp::dev::LocalizationStatusEnum& status) override;

    /**
    * Gets a set of pose estimates computed by the localization algorithm.
    * @return true/false
    */
    bool   getEstimatedPoses(std::vector<yarp::dev::Map2DLocation>& poses) override;

    /**
    * Gets the current position of the robot w.r.t world reference frame
    * @param loc the location of the robot
    * @return true/false
    */
    bool   getCurrentPosition(yarp::dev::Map2DLocation& loc) override;

    /**
    * Sets the initial pose for the localization algorithm which estimates the current position of the robot w.r.t world reference frame.
    * @param loc the location of the robot
    * @return true/false
    */
    bool   setInitialPose(yarp::dev::Map2DLocation& loc) override;
};

class pozyxLocalizerThread : public yarp::os::PeriodicThread
{
protected:
    //general
    double                       m_last_statistics_printed;
    yarp::dev::Map2DLocation     m_initial_loc;
    yarp::dev::Map2DLocation     m_localization_data;
    yarp::dev::Map2DLocation     m_pozyx_data;
    yarp::os::Mutex              m_mutex;
    yarp::os::Searchable&        m_cfg;
    std::string                  m_local_name;

public:
    pozyxLocalizerThread(double _period, yarp::os::Searchable& _cfg);
    virtual bool threadInit() override;
    virtual void threadRelease() override;
    virtual void run() override;

public:
    bool initializeLocalization(yarp::dev::Map2DLocation& loc);
    bool getCurrentLoc(yarp::dev::Map2DLocation& loc);
};

#endif