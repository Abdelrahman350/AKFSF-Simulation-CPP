// ------------------------------------------------------------------------------- //
// Advanced Kalman Filtering and Sensor Fusion Course - Extended Kalman Filter
//
// ####### STUDENT FILE #######
//
// Usage:
// -Rename this file to "kalmanfilter.cpp" if you want to use this code.

#include "kalmanfilter.h"
#include "utils.h"

// -------------------------------------------------- //
// YOU CAN USE AND MODIFY THESE CONSTANTS HERE
constexpr double ACCEL_STD = 1.0;
constexpr double GYRO_STD = 0.01/180.0 * M_PI;
constexpr double INIT_VEL_STD = 10.0;
constexpr double INIT_PSI_STD = 45.0/180.0 * M_PI;
constexpr double GPS_POS_STD = 3.0;
constexpr double LIDAR_RANGE_STD = 3.0;
constexpr double LIDAR_THETA_STD = 0.02;
// -------------------------------------------------- //

void KalmanFilter::handleLidarMeasurements(const std::vector<LidarMeasurement>& dataset, const BeaconMap& map)
{
    // Assume No Correlation between the Measurements and Update Sequentially
    for(const auto& meas : dataset) {handleLidarMeasurement(meas, map);}
}

void KalmanFilter::handleLidarMeasurement(LidarMeasurement meas, const BeaconMap& map)
{
    if (isInitialised())
    {
        VectorXd state = getState();
        MatrixXd cov = getCovariance();

        // Implement The Kalman Filter Update Step for the Lidar Measurements in the 
        // section below.
        // HINT: use the wrapAngle() function on angular values to always keep angle
        // values within correct range, otherwise strange angle effects might be seen.
        // HINT: You can use the constants: LIDAR_RANGE_STD, LIDAR_THETA_STD
        // HINT: The mapped-matched beacon position can be accessed by the variables
        // map_beacon.x and map_beacon.y
        // ----------------------------------------------------------------------- //
        // ENTER YOUR CODE HERE

        BeaconData map_beacon = map.getBeaconWithId(meas.id); // Match Beacon with built in Data Association Id
        if (meas.id != -1 && map_beacon.id != -1)
        {           
            // The map matched beacon positions can be accessed using: map_beacon.x AND map_beacon.y
            // Get state vector quantities
            double p_x = state(0);
            double p_y = state(1);
            double psi = state(2);
            double v = state(3);

            // True measurement vector (from sensor)
            Vector2d z = Vector2d::Zero();
            z << meas.range, meas.theta;

            // Predicted measurement vector (from measurement model)
            Vector2d z_hat = Vector2d::Zero();
            double dx = map_beacon.x - p_x;
            double dy = map_beacon.y - p_y;
            double r_hat = sqrt(dx*dx + dy*dy);
            double theta_hat = wrapAngle(atan2(dy, dx) - psi);
            z_hat << r_hat, theta_hat;

            // Measurement model sensitivity matrix
            MatrixXd H = MatrixXd::Zero(2, 4);
            H(0, 0) = -dx / r_hat;
            H(0, 1) = -dy / r_hat;
            H(1, 0) = dy / (r_hat*r_hat);
            H(1, 1) = -dx / (r_hat*r_hat);
            H(1, 2) = -1;

            // Measurement model covariance matrix
            Matrix2d R = Matrix2d::Zero();
            R(0, 0) = LIDAR_RANGE_STD*LIDAR_RANGE_STD;
            R(1, 1) = LIDAR_THETA_STD*LIDAR_THETA_STD;

            // Calculate innovation mean and innovation covariance
            Vector2d innovation = z - z_hat;
            Matrix2d S = H*cov*H.transpose() + R;

            // Calculate Kalman gain
            MatrixXd K = cov * H.transpose() * S.inverse();
            
            // Wrap the heading innovation
            innovation(1) = wrapAngle(innovation(1));

            // Update state vector and covariance
            state = state + K * innovation;
            cov = (Matrix4d::Identity() - K*H) * cov;
        }

        // ----------------------------------------------------------------------- //

        setState(state);
        setCovariance(cov);
    }
}

void KalmanFilter::predictionStep(GyroMeasurement gyro, double dt)
{
    if (isInitialised())
    {
        VectorXd state = getState();
        MatrixXd cov = getCovariance();

        // Implement The Kalman Filter Prediction Step for the system in the  
        // section below.
        // HINT: Assume the state vector has the form [PX, PY, PSI, V].
        // HINT: Use the Gyroscope measurement as an input into the prediction step.
        // HINT: You can use the constants: ACCEL_STD, GYRO_STD
        // HINT: use the wrapAngle() function on angular values to always keep angle
        // values within correct range, otherwise strange angle effects might be seen.
        // ----------------------------------------------------------------------- //
        // ENTER YOUR CODE HERE
        double p_x = state(0);
        double p_y = state(1);
        double psi = state(2);
        double v = state(3);

        // Predicting new states
        p_x += dt * v * cos(psi);
        p_y += dt * v * sin(psi);
        psi = wrapAngle(psi + dt * gyro.psi_dot);
        state << p_x, p_y, psi, v;

        // Calculate process Jacobian matrix (Napla)F
        Matrix4d F = Matrix4d::Identity();
        F(0, 2) = -dt * v * sin(psi);
        F(0, 3) = dt * cos(psi);

        F(1, 2) = dt * v * cos(psi);
        F(1, 3) = dt * sin(psi);

        // Calculate covariance matrix Q
        Matrix4d Q = Matrix4d::Zero();
        Q(2, 2) = dt*dt * GYRO_STD*GYRO_STD;
        Q(3, 3) = dt*dt * ACCEL_STD*ACCEL_STD;

        // Predicting new covariance
        cov = F * cov * F.transpose() + Q;

        // ----------------------------------------------------------------------- //

        setState(state);
        setCovariance(cov);
    } 
}

void KalmanFilter::handleGPSMeasurement(GPSMeasurement meas)
{
    // All this code is the same as the LKF as the measurement model is linear
    // so the EKF update state would just produce the same result.
    if(isInitialised())
    {
        VectorXd state = getState();
        MatrixXd cov = getCovariance();

        VectorXd z = Vector2d::Zero();
        MatrixXd H = MatrixXd(2,4);
        MatrixXd R = Matrix2d::Zero();

        z << meas.x,meas.y;
        H << 1,0,0,0,0,1,0,0;
        R(0,0) = GPS_POS_STD*GPS_POS_STD;
        R(1,1) = GPS_POS_STD*GPS_POS_STD;

        VectorXd z_hat = H * state;
        VectorXd y = z - z_hat;
        MatrixXd S = H * cov * H.transpose() + R;
        MatrixXd K = cov*H.transpose()*S.inverse();

        state = state + K*y;
        cov = (Matrix4d::Identity() - K*H) * cov;

        setState(state);
        setCovariance(cov);
    }
    else
    {
        VectorXd state = Vector4d::Zero();
        MatrixXd cov = Matrix4d::Zero();

        state(0) = meas.x;
        state(1) = meas.y;
        cov(0,0) = GPS_POS_STD*GPS_POS_STD;
        cov(1,1) = GPS_POS_STD*GPS_POS_STD;
        cov(2,2) = INIT_PSI_STD*INIT_PSI_STD;
        cov(3,3) = INIT_VEL_STD*INIT_VEL_STD;

        setState(state);
        setCovariance(cov);
    } 
             
}

Matrix2d KalmanFilter::getVehicleStatePositionCovariance()
{
    Matrix2d pos_cov = Matrix2d::Zero();
    MatrixXd cov = getCovariance();
    if (isInitialised() && cov.size() != 0){pos_cov << cov(0,0), cov(0,1), cov(1,0), cov(1,1);}
    return pos_cov;
}

VehicleState KalmanFilter::getVehicleState()
{
    if (isInitialised())
    {
        VectorXd state = getState(); // STATE VECTOR [X,Y,PSI,V,...]
        return VehicleState(state[0],state[1],state[2],state[3]);
    }
    return VehicleState();
}

void KalmanFilter::predictionStep(double dt){}