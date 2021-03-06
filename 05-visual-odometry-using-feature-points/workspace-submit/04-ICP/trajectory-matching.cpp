#include <unistd.h>
// c++ standard library:
#include <string>
#include <iostream>
#include <fstream>
// eigen for matrix algebra:
#include <Eigen/Core>
#include <Eigen/Dense>
#include <Eigen/Geometry>
// matrix lie algebra:
#include <sophus/se3.h>
// pangolin for visualization
#include <pangolin/pangolin.h>

using namespace std;

// path to aligned trajectories file:
string aligned_trajectories_file = "./compare.txt";

void LoadTrajectory(
    const string &aligned_trajectories_file,
    vector<Sophus::SE3, Eigen::aligned_allocator<Sophus::SE3>> &estimated,
    vector<Sophus::SE3, Eigen::aligned_allocator<Sophus::SE3>> &ground_truth     
);

void EstimateTransform(
    const vector<Sophus::SE3, Eigen::aligned_allocator<Sophus::SE3>> &estimated,
    const vector<Sophus::SE3, Eigen::aligned_allocator<Sophus::SE3>> &ground_truth,
    Eigen::Matrix3d &R, Eigen::Vector3d &t    
);

// function for plotting trajectory, don't edit this code
// start point is red and end point is blue
void DrawTrajectory(
    const vector<Sophus::SE3, Eigen::aligned_allocator<Sophus::SE3>> &estimated,
    const vector<Sophus::SE3, Eigen::aligned_allocator<Sophus::SE3>> &ground_truth,
    const Eigen::Matrix3d &R, const Eigen::Vector3d &t    
);

int main(int argc, char **argv) {
    // estimated and ground truth trajectories:
    vector<Sophus::SE3, Eigen::aligned_allocator<Sophus::SE3>> estimated;
    vector<Sophus::SE3, Eigen::aligned_allocator<Sophus::SE3>> ground_truth;

    // load trajectories:
    LoadTrajectory(
        aligned_trajectories_file,
        estimated, ground_truth     
    );

    // estimate transform:
    Eigen::Matrix3d R; Eigen::Vector3d t;
    EstimateTransform(estimated, ground_truth, R, t);

    // draw trajectory in pangolin
    DrawTrajectory(estimated, ground_truth, R, t);  
    
    return 0;
}

/*******************************************************************************************/
void LoadTrajectory(
    const string &aligned_trajectories_file,
    vector<Sophus::SE3, Eigen::aligned_allocator<Sophus::SE3>> &estimated,
    vector<Sophus::SE3, Eigen::aligned_allocator<Sophus::SE3>> &ground_truth     
) {
    // memory buffer:
    Eigen::Vector3d te, tg;
    Eigen::Quaterniond qe, qg;

    // trajectory reader:
    ifstream trajectory(aligned_trajectories_file);
    const int D = 16;
    double value[D]; int d = 0;

    while (trajectory >> value[d]) {
        // update index:
        d = (d + 1) % D;

        if (0 == d) {
            // estimated pose:
            te.x() = value[1];
            te.y() = value[2];
            te.z() = value[3];
            qe.x() = value[4];
            qe.y() = value[5];
            qe.z() = value[6];
            qe.w() = value[7];
            // ground truth pose:
            tg.x() = value[9];
            tg.y() = value[10];
            tg.z() = value[11];
            qg.x() = value[12];
            qg.y() = value[13];
            qg.z() = value[14];
            qg.w() = value[15];           

            estimated.push_back(Sophus::SE3(qe, te));
            ground_truth.push_back(Sophus::SE3(qg, tg));
        }
    }
}

void EstimateTransform(
    const vector<Sophus::SE3, Eigen::aligned_allocator<Sophus::SE3>> &estimated,
    const vector<Sophus::SE3, Eigen::aligned_allocator<Sophus::SE3>> &ground_truth,
    Eigen::Matrix3d &R, Eigen::Vector3d &t    
) {
    // total number of observations:
    const int N = ground_truth.size();

    // centers of estimated and ground truth trajectories:
    Eigen::Vector3d ce = Eigen::Vector3d::Zero(), cg = Eigen::Vector3d::Zero();
    for (int i = 0; i < N; ++i) {
        ce += estimated[i].translation();
        cg += ground_truth[i].translation();
    }
    ce /= N; cg /= N;

    // get rotations:
    Eigen::Matrix3d W = Eigen::Matrix3d::Zero();
    for (int i = 0; i < N; ++i) {
        auto qe = estimated[i].translation() - ce;
        auto qg = ground_truth[i].translation() - cg;
        W += qg * qe.transpose();
    }
    Eigen::JacobiSVD<Eigen::Matrix3d> svd(W, Eigen::ComputeFullV | Eigen::ComputeFullU);
    R = svd.matrixU() * svd.matrixV().transpose();

    // get translation:
    t = cg - R * ce;
}

void DrawTrajectory(
    const vector<Sophus::SE3, Eigen::aligned_allocator<Sophus::SE3>> &estimated,
    const vector<Sophus::SE3, Eigen::aligned_allocator<Sophus::SE3>> &ground_truth, 
    const Eigen::Matrix3d &R, const Eigen::Vector3d &t
) {
    if (estimated.empty() || ground_truth.empty() || estimated.size() != ground_truth.size()) {
        cerr << "The trajectories are not aligned!" << endl;
        return;
    }

    // create pangolin window and plot the trajectory
    pangolin::CreateWindowAndBind("Trajectory Viewer", 1024, 768);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    pangolin::OpenGlRenderState s_cam(
            pangolin::ProjectionMatrix(1024, 768, 500, 500, 512, 389, 0.1, 1000),
            pangolin::ModelViewLookAt(0, -0.1, -1.8, 0, 0, 0, 0.0, -1.0, 0.0)
    );

    pangolin::View &d_cam = pangolin::CreateDisplay()
            .SetBounds(0.0, 1.0, pangolin::Attach::Pix(175), 1.0, -1024.0f / 768.0f)
            .SetHandler(new pangolin::Handler3D(s_cam));


    while (pangolin::ShouldQuit() == false) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        d_cam.Activate(s_cam);
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

        glLineWidth(2);
        const int N = ground_truth.size();
        for (size_t i = 0; i < N - 1; i++) {
            // estimated:
            glColor3f(0.0f, 0.0f, 1.0f);
            glBegin(GL_LINES);
            auto te1 = R * estimated[i].translation() + t;
            auto te2 = R * estimated[i + 1].translation() + t;
            glVertex3d(te1[0], te1[1], te1[2]);
            glVertex3d(te2[0], te2[1], te2[2]);
            glEnd();
            // ground truth:
            glColor3f(1.0f, 0.0f, 0.0f);
            glBegin(GL_LINES);
            auto tg1 = ground_truth[i].translation();
            auto tg2 = ground_truth[i + 1].translation();
            glVertex3d(tg1[0], tg1[1], tg1[2]);
            glVertex3d(tg2[0], tg2[1], tg2[2]);
            glEnd();            
        }
        pangolin::FinishFrame();
    }
}