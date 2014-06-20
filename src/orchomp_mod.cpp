/** \file orchomp_mod.cpp
 * \brief Implementation of the orchomp module, an implementation of CHOMP
 *        using libcd.
 * \author Christopher Dellin
 * \date 2012
 */

/* (C) Copyright 2012-2013 Carnegie Mellon University */

/* This module (orchomp) is part of libcd.
 *
 * This module of libcd is free software: you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This module of libcd is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * A copy of the GNU General Public License is provided with libcd
 * (license-gpl.txt) and is also available at <http://www.gnu.org/licenses/>.
 */

#include <time.h>
#include "orchomp_mod.h"
#include "orchomp_kdata.h"

#define DEBUG_TIMING

#ifdef DEBUG_TIMING
#  define TIC() { struct timespec tic; struct timespec toc; clock_gettime(CLOCK_THREAD_CPUTIME_ID, &tic);
#  define TOC(tsptr) clock_gettime(CLOCK_THREAD_CPUTIME_ID, &toc); CD_OS_TIMESPEC_SUB(&toc, &tic); CD_OS_TIMESPEC_ADD(tsptr, &toc); }
#else
#  define TIC()
#  define TOC(tsptr)
#endif


namespace orchomp
{

bool mod::isWithinPaddedLimits( const chomp::MatX & mat ) const{
    assert( upperJointLimits.size() > 0 );
    assert( lowerJointLimits.size() > 0 );
    assert( mat.cols() > 0 );

    for ( int i = 0; i < mat.cols(); i ++ ){
        if ( mat(i) > paddedUpperJointLimits[i] ||
             mat(i) < paddedLowerJointLimits[i] ){
            return false;
        }
    }
    return true;
}
bool mod::isWithinLimits( const chomp::MatX & mat ) const{
    assert( upperJointLimits.size() > 0 );
    assert( lowerJointLimits.size() > 0 );
    assert( mat.cols() > 0 );

    for ( int i = 0; i < mat.cols(); i ++ ){
        if ( mat(i) > upperJointLimits[i] ||
             mat(i) < lowerJointLimits[i] ){
            return false;
        }
    }
    return true;
}

void mod::coutTrajectory() const
{
    for ( int i = 0; i < trajectory.rows(); i ++ ){


        for ( int j = 0; j < trajectory.cols() ; j ++ ){

            std::cout << trajectory( i, j ) << "\t";
        }
        std::cout << "\n";
    }
}
void mod::isTrajectoryWithinLimits() const {
    for( int i = 0; i < trajectory.rows(); i ++ ){
        chomp::MatX test = trajectory.row(i);
        assert( isWithinLimits( test ) );
        //debugStream << "Point " << i << " is within limits" << std::endl;
    }
}

int mod::playback(std::ostream& sout, std::istream& sinput)
{

    
    trajectory = chomper->xi;

    for ( int i = 0; i < trajectory.rows(); i ++ ){

        std::vector< OpenRAVE::dReal > vec;
        getStateAsVector( trajectory.row(i), vec );
        
        viewspheresVec( trajectory.row(i), vec, 0.05 );

    }

    return 0;
}
//constructor that registers all of the commands to the openRave
//   command line interface.
mod::mod(OpenRAVE::EnvironmentBasePtr penv) :
    OpenRAVE::ModuleBase(penv), environment( penv ), 
    factory( NULL ), sphere_collider( NULL ),
    collisionHelper( NULL ), chomper( NULL )
{
      __description = "orchomp: implementation multigrid chomp";
      RegisterCommand("viewspheres",
              boost::bind(&mod::viewspheres,this,_1,_2),
              "view spheres");
      RegisterCommand("computedistancefield",
               boost::bind(&mod::computedistancefield,this,_1,_2),
               "compute distance field");
      RegisterCommand("addfield_fromobsarray",
            boost::bind( &mod::addfield_fromobsarray,this,_1,_2),
            "compute distance field");
      RegisterCommand("create", 
            boost::bind(&mod::create,this,_1,_2),
            "create a chomp run");
      RegisterCommand("iterate",
            boost::bind(&mod::iterate,this,_1,_2),
            "create a chomp run");
      RegisterCommand("gettraj",
            boost::bind(&mod::gettraj,this,_1,_2),
            "create a chomp run");
      RegisterCommand("destroy",
            boost::bind(&mod::destroy,this,_1,_2),
            "create a chomp run");
       RegisterCommand("execute",
            boost::bind(&mod::execute,this,_1,_2),
            "play a trajectory on a robot");
       RegisterCommand("playback",
            boost::bind(&mod::playback,this,_1,_2),
            "playback a trajectory on a robot");

}

/* ======================================================================== *
 * module commands
 */

//view the collision geometry.
int mod::viewspheres(std::ostream& sout, std::istream& sinput)
{

    
    OpenRAVE::EnvironmentMutex::scoped_lock lockenv(environment->GetMutex());
    parseViewSpheres( sout,  sinput);

    if ( !sphere_collider ) { getSpheres() ; }
    
    char text_buf[1024];
    
    const size_t n_active = active_spheres.size();
    const size_t n_inactive = inactive_spheres.size();

    for ( size_t i=0; i < n_active + n_inactive ; i ++ )
    {
        //extract the current sphere
        //  if i is less than n_active, get a sphere from active_spheres,
        //  else, get a sphere from inactive_spheres
        const Sphere & sphere = (i < n_active ?
                                 active_spheres[i] :
                                 inactive_spheres[i-n_active]);

        //make a kinbody sphere object to correspond to this sphere.
        OpenRAVE::KinBodyPtr sbody = 
                OpenRAVE::RaveCreateKinBody( environment );
        sprintf( text_buf, "orcdchomp_sphere_%d", int(i));
        sbody->SetName(text_buf);
        

        //set the dimensions and transform of the sphere.
        std::vector< OpenRAVE::Vector > svec;

        //get the position of the sphere in the world 
        OpenRAVE::Transform t = 
                sphere.body->GetLink(sphere.linkname)->GetTransform();
        OpenRAVE::Vector v = t * OpenRAVE::Vector(sphere.pose);
        
        //set the radius of the sphere
        v.w = sphere.radius; 

        //give the kinbody the sphere parameters.
        svec.push_back(v);
        sbody->InitFromSpheres(svec, true);

        environment->Add( sbody );
    }
    return 0;
}

//view the collision geometry. .
int mod::viewspheresVec(const chomp::MatX & q,
                        const std::vector< OpenRAVE::dReal > & vec, 
                        double time)
{

    struct timespec ticks_tic;
    struct timespec ticks_toc;

    /* start timing voxel grid computation */
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ticks_tic);


    robot->SetActiveDOFValues( vec );

    if ( !sphere_collider ) { getSpheres() ; }
    
    char text_buf[1024];
    
    const size_t n_active = active_spheres.size();
    const size_t n_inactive = inactive_spheres.size();

    std::vector< OpenRAVE::KinBodyPtr > bodies;

    for ( size_t i=0; i < n_active + n_inactive ; i ++ )
    {

        double cost;
        chomp::MatX dxdq, cgrad;

        if( sphere_collider ){ 
            cost = sphere_collider->getCost( q, i, dxdq, cgrad );
            if ( cost <= 0.5*sphere_collider->epsilon &&
                 cost <= 0.5*sphere_collider->epsilon_self){
                continue; }
        }
        
        //extract the current sphere
        //  if i is less than n_active, get a sphere from active_spheres,
        //  else, get a sphere from inactive_spheres
        const Sphere & current_sphere = (i < n_active ?
                                 active_spheres[i] :
                                 inactive_spheres[i-n_active]);

        //make a kinbody sphere object to correspond to this sphere.
        OpenRAVE::KinBodyPtr sbody = 
                OpenRAVE::RaveCreateKinBody( environment );
        sprintf( text_buf, "orcdchomp_sphere_%d", int(i));
        sbody->SetName(text_buf);
        
        //set the dimensions and transform of the sphere.
        std::vector< OpenRAVE::Vector > svec;

        //get the position of the sphere in the world 
        OpenRAVE::Transform t =  current_sphere.body->GetLink(
                                 current_sphere.linkname)->GetTransform();
        OpenRAVE::Vector position = t * OpenRAVE::Vector(current_sphere.pose);
        
        //set the radius of the sphere
        position.w = current_sphere.radius; 

        //give the kinbody the sphere parameters.
        svec.push_back( position );
        sbody->InitFromSpheres(svec, true);
        
        bodies.push_back( sbody );
        environment->Add( sbody );
       
    }
    
    while ( true ){
      /* stop timing voxel grid computation */
      clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ticks_toc);
      CD_OS_TIMESPEC_SUB(&ticks_toc, &ticks_tic);
      if ( time < CD_OS_TIMESPEC_DOUBLE(&ticks_toc) ){ break ; }
    }

    for ( size_t i = 0; i < bodies.size() ; i ++ ){
        environment->Remove( bodies[i] );
    }
    
    return 0;
}




/* computedistancefield robot Herb2
 * computes a distance field in the vicinity of the passed kinbody
 * 
 * note: does aabb include disabled bodies? probably, but we might hope not ...
 * */

 // NOTES : this is likely to not work for several reasons:
 //         1. It is lacking the necessary libraries for computing
 //         2. Even if the libraries were correct, it is unlikely to work
 //             with the current chomp style of gradients
int mod::computedistancefield(std::ostream& sout, std::istream& sinput)
{
    
    //TODO uncomment the lock environment line.
    //lock the environment
    //OpenRAVE::EnvironmentMutex::scoped_lock(environment->GetMutex());
    
    //Parse the arguments
    parseComputeDistanceField( sout,  sinput);
    //currently, parse compute does all of the actual work for this function.
    //  That seems peculiar.
    

    return 0;
}


int mod::addfield_fromobsarray(std::ostream& sout, std::istream& sinput)
{
    parseAddFieldFromObsArray( sout,  sinput);
   
    return 0;

}




/* runchomp robot Herb2
 * run chomp from the current config to the passed goal config
 * uses the active dofs of the passed robot
 * initialized with a straight-line trajectory
 * */
int mod::create(std::ostream& sout, std::istream& sinput)
{
    //get the lock for the environment
    OpenRAVE::EnvironmentMutex::scoped_lock lockenv(
              environment->GetMutex() );

    std::cout << "Creating" << std::endl;

    parseCreate( sout,  sinput);
    

    //after the arguments have been collected, pass them to chomp
    createInitialTrajectory();
    
    //create a padded upper and lower joint limits vectors. These will be
    //  Used for constraining the trajectory to the joint limits.
    //  Since sometimes, chomp allows constraints to be minutely off,
    //  this will prevent that from happening.
    paddedUpperJointLimits.resize( upperJointLimits.size() );
    paddedLowerJointLimits.resize( lowerJointLimits.size() );
    for ( size_t i = 0; i < upperJointLimits.size(); i ++ ){
        OpenRAVE::dReal interval = upperJointLimits[i] - lowerJointLimits[i];
        interval *= info.jointPadding;

        //fill the padded joint limits
        paddedUpperJointLimits[i] = upperJointLimits[i] - interval;
        paddedLowerJointLimits[i] = lowerJointLimits[i] + interval;
    }

    //TODO remove the clamp to limits thing.
    clampToLimits(q0);
    clampToLimits(q1);

    assert( isWithinPaddedLimits( q0 ) );
    assert( isWithinPaddedLimits( q1 ) );


    if ( !info.noFactory ){
        factory = new ORConstraintFactory( this );
    }
    //now that we have a trajectory, make a chomp object
    chomper = new chomp::Chomp( factory, trajectory,
                                q0, q1, info.n_max, 
                                info.alpha, info.obstol,
                                info.max_global_iter,
                                info.max_local_iter,
                                info.t_total);

     
    //TODO Compute signed distance field
    

    //Setup the collision geometry
    getSpheres();

    //create the sphere collider to actually 
    //  use the sphere information, and pass in its parameters.
    if ( !info.noCollider ){
        sphere_collider = new SphereCollisionHelper(
                          n_dof, 3, active_spheres.size(), this);
        sphere_collider->epsilon = info.epsilon;
        sphere_collider->epsilon_self = info.epsilon_self;
        sphere_collider->obs_factor = info.obs_factor;
        sphere_collider->obs_factor_self = info.obs_factor_self;
    }

    //TODO setup momentum stuff ?? maybe ?? 

    //give chomp a collision helper to 
    //deal with collision and gradients.
    if (sphere_collider){
        collisionHelper = new chomp::ChompCollGradHelper(
                                sphere_collider, info.gamma);
        chomper->ghelper = collisionHelper;
    }
    
    //if we want a debug observer, then give chomp the
    //  link to the observer.
    if ( info.doObserve ){
        chomper->observer = &observer;
    }
    
    std::cout << "Done Creating" << std::endl;
    
    return 0;
}

int mod::iterate(std::ostream& sout, std::istream& sinput)
{
    std::cout << "Iterating" << std::endl;
    
    //get the arguments
    parseIterate( sout,  sinput);

    //get the lock for the environment
    OpenRAVE::EnvironmentMutex::scoped_lock lock(environment->GetMutex() );

    if (!robot.get() ){
        robot = environment->GetRobot( robot_name.c_str() );
    }

    //solve chomp
    chomper->solve( info.doGlobal, info.doLocal );

    std::cout << "Done Iterating" << std::endl;
    return 0;
}

int mod::gettraj(std::ostream& sout, std::istream& sinput)
{
    
    //get the trajectory from the chomp.
    trajectory = chomper->xi;

    std::cout << "Getting Trajectory" << std::endl;
    OpenRAVE::EnvironmentMutex::scoped_lock lockenv;

    parseGetTraj( sout,  sinput);
    
    //get the lock for the environment
    lockenv = OpenRAVE::EnvironmentMutex::scoped_lock(
              environment->GetMutex() );
   

    debugStream << "Checking Trajectory" << std::endl;
    //check and or print the trajectory
    //isTrajectoryWithinLimits();
    //coutTrajectory();

    //setup the openrave trajectory pointer to receive the
    //  found trajectory.
    trajectory_ptr = OpenRAVE::RaveCreateTrajectory(environment);

    if (!robot.get() ){
        robot = environment->GetRobot( robot_name.c_str() );
    }
    trajectory_ptr->Init(
        robot->GetActiveConfigurationSpecification());

    debugStream << "Done locking environment,"
                << " begun extracting trajectory" << std::endl;
    
    
    //For every state (each row is a state), extract the data,
    //  and turn it into a vector, then give it to


    //get the start state as an openrave vector
    std::vector< OpenRAVE::dReal > startState;
    getStateAsVector( q0, startState );

    //insert the start state into the trajectory
    trajectory_ptr->Insert( 0, startState );

    //get the rest of the trajectory
    for ( int i = 0; i < trajectory.rows(); i ++ ){
        
        std::vector< OpenRAVE::dReal > state;
        getIthStateAsVector( i, state );
        trajectory_ptr->Insert( i + 1, state );
        
    }
    
    //get the start state as an openrave vector
    std::vector< OpenRAVE::dReal > endState;
    getStateAsVector( q1, endState );

    //insert the start state into the trajectory
    trajectory_ptr->Insert( trajectory.rows(), endState );


    //this is about changing the timing or something
    /*
       OpenRAVE::planningutils::RetimeActiveDOFTrajectory
                (traj_ptr, boostrobot, false, 0.2, 0.2,
                 "LinearTrajectoryRetimer","");
    */
    
    //TODO : check for collisions
    
    debugStream << "Retiming Trajectory" << std::endl;
    //this times the trajectory so that it can be
    //  sent to a planner
    OpenRAVE::planningutils::RetimeActiveDOFTrajectory(
                             trajectory_ptr, robot);

    debugStream << "Serializing trajectory output" << std::endl; 
    //serialize the trajectory and send it over the 
    //  output stream.
    trajectory_ptr->serialize( sout );

    std::cout << "Done Getting Trajectory" << std::endl;


    return 0;
}
int mod::execute(std::ostream& sout, std::istream& sinput){

    std::cout << "Executing" << std::endl;
    //get the lock for the environment
    OpenRAVE::EnvironmentMutex::scoped_lock lockenv(
              environment->GetMutex() );


    if ( trajectory_ptr.get() ){
        robot->GetController()->SetPath(trajectory_ptr);
        //robot->WaitForController(0);
    }
    else {
        RAVELOG_ERROR("There is no trajectory to run.\n");
    }
    
    std::cout << "Done Executing" << std::endl;
    return 0;
        
}

int mod::destroy(std::ostream& sout, std::istream& sinput){
    
    if (chomper){ delete chomper; }
    if (sphere_collider){ delete sphere_collider; }
    if (factory){ delete factory;}
    if (collisionHelper){ delete collisionHelper; }

    return 0;
}



//takes the two endpoints and fills the trajectory matrix by
//  linearly interpolating between the two.
inline void mod::createInitialTrajectory()
{

    //make sure that the number of points is not zero
    assert( info.n != 0 );
    //make sure that the start and endpoint are the same size
    assert( q0.size() == q1.size() );

    //resize the trajectory to hold the current endpoint
    trajectory.resize(info.n, q0.size() );

    //fill the trajectory matrix 
    for (size_t i=0; i<info.n; ++i) {
        trajectory.row(i) = (i+1)*(q1-q0)/(info.n+1) + q0;

        chomp::MatX test = trajectory.row(i);

        //make sure that all of the points are within the joint limits
        //  this is unnecessary, but nice for now.
        //  TODO remove this.
        assert( isWithinLimits( test ));
    }
}

inline void mod::clampToLimits( chomp::MatX & state ){
    for ( int i = 0; i < state.cols() ; i ++ ){
        if ( state(i) > paddedUpperJointLimits[i] ){
            state(i) = paddedUpperJointLimits[i];
        }
        else if ( state(i) < paddedLowerJointLimits[i] ){
            state(i) = paddedLowerJointLimits[i];
        }
    }
}



void mod::getSpheres()
{
    
    
    //a vector holding all of the pertinent bodies in the scene
    std::vector<OpenRAVE::KinBodyPtr> bodies;

    /* consider the robot kinbody, as well as all grabbed bodies */
    robot->GetGrabbed(bodies);
    bodies.push_back(environment->GetRobot( robot->GetName() ));
    
    //iterate over all of the bodies.
    for (size_t i=0; i < bodies.size(); i++)
    {
        OpenRAVE::KinBodyPtr body = bodies[i];

        //get the spheres of the body by creating an xml reader to
        //  extract the spheres from the xml files of the objects
        boost::shared_ptr<kdata> data_reader = 
            boost::dynamic_pointer_cast<kdata>
                (body->GetReadableInterface("orcdchomp"));
         
        //bail if there is no orcdchomp data.
        if (data_reader.get() == NULL ) {
            debugStream << "Failed to get: " << body->GetName() << std::endl;
            continue;

            throw OpenRAVE::openrave_exception(
                "kinbody does not have a <orcdchomp> tag defined!");
        }
        
        
        for (size_t j = 0; j < data_reader->spheres.size(); j++ )
        {
            
            Sphere & sphere = data_reader->spheres[j];
            
            sphere.body = body.get();
            /* what robot link is this sphere attached to? */
            if (body.get() == robot.get()){
                
                //TODO THIS IS AN OUTRAGEOUS HACK PLEASE make it not necessary
                if ( sphere.linkname == "/right/wam0" ){
                    sphere.linkname = "/right/wam2";
                }else if ( sphere.linkname == "/left/wam0" ){
                    sphere.linkname = "/left/wam2";
                }

                sphere.link = robot->GetLink(sphere.linkname).get();
            }
            //the sphere is attached to a grabbed kinbody
            else{
                sphere.link = robot->IsGrabbing(body).get();
            }

            //if the link does not exist, throw an exception
            if(!sphere.link){ 
               throw OPENRAVE_EXCEPTION_FORMAT(
                     "link %s in <orcdchomp> does not exist.",
                     sphere.linkname, OpenRAVE::ORE_Failed);
            }
            
            sphere.linkindex = sphere.link->GetIndex();
            
            //if the body is not the robot, then get the transform
            if ( body.get() != robot.get() )
            {
                OpenRAVE::Transform T_w_klink = 
                    body->GetLink(sphere.linkname)->GetTransform();
                OpenRAVE::Transform T_w_rlink = sphere.link->GetTransform();
                OpenRAVE::Vector v = T_w_rlink.inverse()
                                     * T_w_klink 
                                     * OpenRAVE::Vector(sphere.pose);
                sphere.pose[0] = v.x;
                sphere.pose[1] = v.y;
                sphere.pose[2] = v.z;
            }
             
            /* is this link affected by the robot's active dofs? */
            for (size_t k = 0; k < n_dof; k++){
                if ( robot->DoesAffect( active_indices[k],
                                        sphere.linkindex    )){
                    active_spheres.push_back( sphere );
                    break;
                }
                //if the sphere is not active, add it to the inactive
                //  vector
                else if ( k == n_dof - 1 ) {
                    inactive_spheres.push_back( sphere);
                }
            }
        }
    }    
}


} /* namespace orchomp */


