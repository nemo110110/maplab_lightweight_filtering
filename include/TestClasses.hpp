/*
 * TestClasses.hpp
 *
 *  Created on: Feb 9, 2014
 *      Author: Bloeschm
 */

#ifndef TestClasses_HPP_
#define TestClasses_HPP_

#include "State.hpp"
#include "Update.hpp"
#include "Prediction.hpp"

namespace LWFTest{

namespace Nonlinear{

class State: public LWF::State<LWF::TH_multiple_elements<LWF::VectorElement<3>,4>,LWF::QuaternionElement>{
 public:
  enum StateNames {
    POS,
    VEL,
    ACB,
    GYB,
    ATT
  };
  State(){
    getName<0>() = "pos";
    getName<1>() = "vel";
    getName<2>() = "acb";
    getName<3>() = "gyb";
    getName<4>() = "att";
  }
  ~State(){};
};
class FilterState: public LWF::FilterStateNew<State,LWF::ModeEKF,false,false,true,false>{};
class UpdateMeas: public LWF::State<LWF::VectorElement<3>,LWF::QuaternionElement>{
 public:
  enum StateNames {
    POS,
    ATT
  };
  UpdateMeas(){};
  ~UpdateMeas(){};
};
class UpdateNoise: public LWF::State<LWF::TH_multiple_elements<LWF::VectorElement<3>,2>>{
 public:
  enum StateNames {
    POS,
    ATT
  };
  UpdateNoise(){};
  ~UpdateNoise(){};
};
class Innovation: public LWF::State<LWF::VectorElement<3>,LWF::QuaternionElement>{
 public:
  enum StateNames {
    POS,
    ATT
  };
  Innovation(){};
  ~Innovation(){};
};
class PredictionNoise: public LWF::State<LWF::TH_multiple_elements<LWF::VectorElement<3>,5>>{
 public:
  enum StateNames {
    POS,
    VEL,
    ACB,
    GYB,
    ATT
  };
  PredictionNoise(){};
  ~PredictionNoise(){};
};
class PredictionMeas: public LWF::State<LWF::TH_multiple_elements<LWF::VectorElement<3>,2>>{
 public:
  enum StateNames {
    ACC,
    GYR
  };
  PredictionMeas(){};
  ~PredictionMeas(){};
};
class OutlierDetectionExample: public LWF::OutlierDetection<LWF::ODEntry<0,3,1>>{
};

class UpdateExample: public LWF::Update<Innovation,FilterState,UpdateMeas,UpdateNoise,OutlierDetectionExample,LWF::DummyPrediction,false>{
 public:
  using LWF::Update<Innovation,FilterState,UpdateMeas,UpdateNoise,OutlierDetectionExample,LWF::DummyPrediction,false>::eval;
  using mtState = LWF::Update<Innovation,FilterState,UpdateMeas,UpdateNoise,OutlierDetectionExample,LWF::DummyPrediction,false>::mtState;
  typedef UpdateMeas mtMeas;
  typedef UpdateNoise mtNoise;
  typedef Innovation mtInnovation;
  UpdateExample(){};
  ~UpdateExample(){};
  void eval(mtInnovation& inn, const mtState& state, const mtMeas& meas, const mtNoise noise, double dt = 0.0) const{
    mtState linState = state;
    state.boxPlus(state.difVecLin_,linState);
    inn.get<Innovation::POS>() = linState.get<State::ATT>().rotate(linState.get<State::POS>())-meas.get<UpdateMeas::POS>()+noise.get<UpdateNoise::POS>();
    inn.get<Innovation::ATT>() = (linState.get<State::ATT>()*meas.get<UpdateMeas::ATT>().inverted()).boxPlus(noise.get<UpdateNoise::ATT>());
  }
  void jacInput(mtJacInput& J, const mtState& state, const mtMeas& meas, double dt = 0.0) const{
    mtState linState = state;
    state.boxPlus(state.difVecLin_,linState);
    mtInnovation inn;
    J.setZero();
    J.template block<3,3>(mtInnovation::getId<Innovation::POS>(),mtState::getId<State::POS>()) = rot::RotationMatrixPD(linState.get<State::ATT>()).matrix();
    J.template block<3,3>(mtInnovation::getId<Innovation::POS>(),mtState::getId<State::ATT>()) = kindr::linear_algebra::getSkewMatrixFromVector(linState.get<State::ATT>().rotate(linState.get<State::POS>()));
    J.template block<3,3>(mtInnovation::getId<Innovation::ATT>(),mtState::getId<State::ATT>()) = Eigen::Matrix3d::Identity();
  }
  void jacNoise(mtJacNoise& J, const mtState& state, const mtMeas& meas, double dt = 0.0) const{
    mtState linState = state;
    state.boxPlus(state.difVecLin_,linState);
    mtInnovation inn;
    J.setZero();
    J.template block<3,3>(mtInnovation::getId<Innovation::POS>(),mtNoise::getId<mtNoise::POS>()) = Eigen::Matrix3d::Identity();
    J.template block<3,3>(mtInnovation::getId<Innovation::ATT>(),mtNoise::getId<mtNoise::ATT>()) = Eigen::Matrix3d::Identity();
  }
};

class PredictionExample: public LWF::Prediction<FilterState,PredictionMeas,PredictionNoise>{
 public:
  using LWF::Prediction<FilterState,PredictionMeas,PredictionNoise>::eval;
  using mtState = LWF::Prediction<FilterState,PredictionMeas,PredictionNoise>::mtState;
  typedef PredictionMeas mtMeas;
  typedef PredictionNoise mtNoise;
  PredictionExample(){};
  ~PredictionExample(){};
  void eval(mtState& output, const mtState& state, const mtMeas& meas, const mtNoise noise, double dt) const{
    Eigen::Vector3d g_(0,0,-9.81);
    Eigen::Vector3d dOmega = -dt*(meas.get<PredictionMeas::GYR>()-state.get<State::GYB>()+noise.get<PredictionNoise::ATT>()/sqrt(dt));
    rot::RotationQuaternionPD dQ = dQ.exponentialMap(dOmega);
    output.get<State::POS>() = (Eigen::Matrix3d::Identity()+kindr::linear_algebra::getSkewMatrixFromVector(dOmega))*state.get<State::POS>()-dt*state.get<State::VEL>()+noise.get<PredictionNoise::POS>()*sqrt(dt);
    output.get<State::VEL>() = (Eigen::Matrix3d::Identity()+kindr::linear_algebra::getSkewMatrixFromVector(dOmega))*state.get<State::VEL>()
        -dt*(meas.get<PredictionMeas::ACC>()-state.get<State::ACB>()+state.get<State::ATT>().inverseRotate(g_)+noise.get<PredictionNoise::VEL>()/sqrt(dt));
    output.get<State::ACB>() = state.get<State::ACB>()+noise.get<PredictionNoise::ACB>()*sqrt(dt);
    output.get<State::GYB>() = state.get<State::GYB>()+noise.get<PredictionNoise::GYB>()*sqrt(dt);
    output.get<State::ATT>() = state.get<State::ATT>()*dQ;
    output.get<State::ATT>().fix();
  }
  void jacInput(mtJacInput& J, const mtState& state, const mtMeas& meas, double dt) const{
    Eigen::Vector3d g_(0,0,-9.81);
    Eigen::Vector3d dOmega = -dt*(meas.get<PredictionMeas::GYR>()-state.get<State::GYB>());
    J.setZero();
    J.template block<3,3>(mtState::getId<State::POS>(),mtState::getId<State::POS>()) = (Eigen::Matrix3d::Identity()+kindr::linear_algebra::getSkewMatrixFromVector(dOmega));
    J.template block<3,3>(mtState::getId<State::POS>(),mtState::getId<State::VEL>()) = -dt*Eigen::Matrix3d::Identity();
    J.template block<3,3>(mtState::getId<State::POS>(),mtState::getId<State::GYB>()) = -dt*kindr::linear_algebra::getSkewMatrixFromVector(state.get<State::POS>());
    J.template block<3,3>(mtState::getId<State::VEL>(),mtState::getId<State::VEL>()) = (Eigen::Matrix3d::Identity()+kindr::linear_algebra::getSkewMatrixFromVector(dOmega));
    J.template block<3,3>(mtState::getId<State::VEL>(),mtState::getId<State::ACB>()) = dt*Eigen::Matrix3d::Identity();
    J.template block<3,3>(mtState::getId<State::VEL>(),mtState::getId<State::GYB>()) = -dt*kindr::linear_algebra::getSkewMatrixFromVector(state.get<State::VEL>());
    J.template block<3,3>(mtState::getId<State::VEL>(),mtState::getId<State::ATT>()) = dt*rot::RotationMatrixPD(state.get<State::ATT>()).matrix().transpose()*kindr::linear_algebra::getSkewMatrixFromVector(g_);
    J.template block<3,3>(mtState::getId<State::ACB>(),mtState::getId<State::ACB>()) = Eigen::Matrix3d::Identity();
    J.template block<3,3>(mtState::getId<State::GYB>(),mtState::getId<State::GYB>()) = Eigen::Matrix3d::Identity();
    J.template block<3,3>(mtState::getId<State::ATT>(),mtState::getId<State::GYB>()) = dt*rot::RotationMatrixPD(state.get<State::ATT>()).matrix()*LWF::Lmat(dOmega);
    J.template block<3,3>(mtState::getId<State::ATT>(),mtState::getId<State::ATT>()) = Eigen::Matrix3d::Identity();
  }
  void jacNoise(mtJacNoise& J, const mtState& state, const mtMeas& meas, double dt) const{
    mtNoise noise;
    Eigen::Vector3d g_(0,0,-9.81);
    Eigen::Vector3d dOmega = -dt*(meas.get<PredictionMeas::GYR>()-state.get<State::GYB>());
    J.setZero();
    J.template block<3,3>(mtState::getId<State::POS>(),mtNoise::getId<mtNoise::POS>()) = Eigen::Matrix3d::Identity()*sqrt(dt);
    J.template block<3,3>(mtState::getId<State::POS>(),mtNoise::getId<mtNoise::ATT>()) = kindr::linear_algebra::getSkewMatrixFromVector(state.get<State::POS>())*sqrt(dt);
    J.template block<3,3>(mtState::getId<State::VEL>(),mtNoise::getId<mtNoise::VEL>()) = -Eigen::Matrix3d::Identity()*sqrt(dt);
    J.template block<3,3>(mtState::getId<State::VEL>(),mtNoise::getId<mtNoise::ATT>()) = kindr::linear_algebra::getSkewMatrixFromVector(state.get<State::VEL>())*sqrt(dt);
    J.template block<3,3>(mtState::getId<State::ACB>(),mtNoise::getId<mtNoise::ACB>()) = Eigen::Matrix3d::Identity()*sqrt(dt);
    J.template block<3,3>(mtState::getId<State::GYB>(),mtNoise::getId<mtNoise::GYB>()) = Eigen::Matrix3d::Identity()*sqrt(dt);
    J.template block<3,3>(mtState::getId<State::ATT>(),mtNoise::getId<mtNoise::ATT>()) = -rot::RotationMatrixPD(state.get<State::ATT>()).matrix()*LWF::Lmat(dOmega)*sqrt(dt);
  }
};

class PredictAndUpdateExample: public LWF::Update<Innovation,FilterState,UpdateMeas,UpdateNoise,OutlierDetectionExample,PredictionExample,true>{
 public:
  using LWF::Update<Innovation,FilterState,UpdateMeas,UpdateNoise,OutlierDetectionExample,PredictionExample,true>::eval;
  using mtState = LWF::Update<Innovation,FilterState,UpdateMeas,UpdateNoise,OutlierDetectionExample,PredictionExample,true>::mtState;
  typedef UpdateMeas mtMeas;
  typedef UpdateNoise mtNoise;
  typedef Innovation mtInnovation;
  PredictAndUpdateExample(){};
  ~PredictAndUpdateExample(){};
  void eval(mtInnovation& inn, const mtState& state, const mtMeas& meas, const mtNoise noise, double dt = 0.0) const{
    inn.get<Innovation::POS>() = state.get<State::ATT>().rotate(state.get<State::POS>())-meas.get<UpdateMeas::POS>()+noise.get<UpdateNoise::POS>();
    inn.get<Innovation::ATT>() = (state.get<State::ATT>()*meas.get<UpdateMeas::ATT>().inverted()).boxPlus(noise.get<UpdateNoise::ATT>());
  }
  void jacInput(mtJacInput& J, const mtState& state, const mtMeas& meas, double dt = 0.0) const{
    mtInnovation inn;
    J.setZero();
    J.template block<3,3>(mtInnovation::getId<Innovation::POS>(),mtState::getId<State::POS>()) = rot::RotationMatrixPD(state.get<State::ATT>()).matrix();
    J.template block<3,3>(mtInnovation::getId<Innovation::POS>(),mtState::getId<State::ATT>()) = kindr::linear_algebra::getSkewMatrixFromVector(state.get<State::ATT>().rotate(state.get<State::POS>()));
    J.template block<3,3>(mtInnovation::getId<Innovation::ATT>(),mtState::getId<State::ATT>()) = Eigen::Matrix3d::Identity();
  }
  void jacNoise(mtJacNoise& J, const mtState& state, const mtMeas& meas, double dt = 0.0) const{
    mtInnovation inn;
    J.setZero();
    J.template block<3,3>(mtInnovation::getId<Innovation::POS>(),mtNoise::getId<mtNoise::POS>()) = Eigen::Matrix3d::Identity();
    J.template block<3,3>(mtInnovation::getId<Innovation::ATT>(),mtNoise::getId<mtNoise::ATT>()) = Eigen::Matrix3d::Identity();
  }
};

}

namespace Linear{

class State: public LWF::State<LWF::TH_multiple_elements<LWF::VectorElement<3>,2>>{
 public:
  enum StateNames {
    POS,
    VEL
  };
  State(){
    createDefaultNames("s");
  };
  ~State(){};
};
class FilterState: public LWF::FilterStateNew<State,LWF::ModeEKF,false,false,true,false>{};
class UpdateMeas: public LWF::State<LWF::ScalarElement,LWF::VectorElement<3>>{
 public:
  enum StateNames {
    HEI,
    POS
  };
  UpdateMeas(){};
  ~UpdateMeas(){};
};
class UpdateNoise: public LWF::State<LWF::ScalarElement,LWF::VectorElement<3>>{
 public:
  enum StateNames {
    HEI,
    POS
  };
  UpdateNoise(){};
  ~UpdateNoise(){};
};
class Innovation: public LWF::State<LWF::ScalarElement,LWF::VectorElement<3>>{
 public:
  enum StateNames {
    HEI,
    POS
  };
  Innovation(){};
  ~Innovation(){};
};
class PredictionNoise: public LWF::State<LWF::VectorElement<3>>{
 public:
  enum StateNames {
    VEL
  };
  PredictionNoise(){};
  ~PredictionNoise(){};
};
class PredictionMeas: public LWF::State<LWF::VectorElement<3>>{
 public:
  enum StateNames {
    ACC
  };
  PredictionMeas(){};
  ~PredictionMeas(){};
};
class OutlierDetectionExample: public LWF::OutlierDetection<LWF::ODEntry<0,3,1>>{
};

class UpdateExample: public LWF::Update<Innovation,FilterState,UpdateMeas,UpdateNoise,OutlierDetectionExample,LWF::DummyPrediction,false>{
 public:
  using LWF::Update<Innovation,FilterState,UpdateMeas,UpdateNoise,OutlierDetectionExample,LWF::DummyPrediction,false>::eval;
  using mtState = LWF::Update<Innovation,FilterState,UpdateMeas,UpdateNoise,OutlierDetectionExample,LWF::DummyPrediction,false>::mtState;
  typedef UpdateMeas mtMeas;
  typedef UpdateNoise mtNoise;
  typedef Innovation mtInnovation;
  UpdateExample(){};
  ~UpdateExample(){};
  void eval(mtInnovation& inn, const mtState& state, const mtMeas& meas, const mtNoise noise, double dt = 0.0) const{
    mtState linState = state;
    state.boxPlus(state.difVecLin_,linState);
    inn.get<Innovation::POS>() = linState.get<State::POS>()-meas.get<UpdateMeas::POS>()+noise.get<UpdateNoise::POS>();
    inn.get<Innovation::HEI>() = Eigen::Vector3d(0,0,1).dot(linState.get<State::POS>())-meas.get<UpdateMeas::HEI>()+noise.get<UpdateNoise::HEI>();;
  }
  void jacInput(mtJacInput& J, const mtState& state, const mtMeas& meas, double dt = 0.0) const{
    mtInnovation inn;
    J.setZero();
    J.template block<3,3>(mtInnovation::getId<Innovation::POS>(),mtState::getId<State::POS>()) = Eigen::Matrix3d::Identity();
    J.template block<1,3>(mtInnovation::getId<Innovation::HEI>(),mtState::getId<State::POS>()) = Eigen::Vector3d(0,0,1).transpose();
  }
  void jacNoise(mtJacNoise& J, const mtState& state, const mtMeas& meas, double dt = 0.0) const{
    mtInnovation inn;
    J.setZero();
    J.template block<3,3>(mtInnovation::getId<Innovation::POS>(),mtNoise::getId<mtNoise::POS>()) = Eigen::Matrix3d::Identity();
    J(mtInnovation::getId<Innovation::HEI>(),mtNoise::getId<mtNoise::HEI>()) = 1.0;
  }
};

class PredictionExample: public LWF::Prediction<FilterState,PredictionMeas,PredictionNoise>{
 public:
  using LWF::Prediction<FilterState,PredictionMeas,PredictionNoise>::eval;
  using mtState = LWF::Prediction<FilterState,PredictionMeas,PredictionNoise>::mtState;
  typedef PredictionMeas mtMeas;
  typedef PredictionNoise mtNoise;
  PredictionExample(){};
  ~PredictionExample(){};
  void eval(mtState& output, const mtState& state, const mtMeas& meas, const mtNoise noise, double dt) const{
    output.get<mtState::POS>() = state.get<mtState::POS>()+dt*state.get<mtState::VEL>();
    output.get<mtState::VEL>()  = state.get<mtState::VEL>()+dt*(meas.get<mtMeas::ACC>()+noise.get<PredictionNoise::VEL>()/sqrt(dt));
  }
  void jacInput(mtJacInput& J, const mtState& state, const mtMeas& meas, double dt) const{
    J.setZero();
    J.template block<3,3>(mtState::getId<State::POS>(),mtState::getId<State::POS>()) = Eigen::Matrix3d::Identity();
    J.template block<3,3>(mtState::getId<State::POS>(),mtState::getId<State::VEL>()) = dt*Eigen::Matrix3d::Identity();
    J.template block<3,3>(mtState::getId<State::VEL>(),mtState::getId<State::VEL>()) = Eigen::Matrix3d::Identity();
  }
  void jacNoise(mtJacNoise& J, const mtState& state, const mtMeas& meas, double dt) const{
    J.setZero();
    J.template block<3,3>(mtState::getId<State::VEL>(),mtNoise::getId<mtNoise::VEL>()) = Eigen::Matrix3d::Identity()*sqrt(dt);
  }
};

class PredictAndUpdateExample: public LWF::Update<Innovation,FilterState,UpdateMeas,UpdateNoise,OutlierDetectionExample,PredictionExample,true>{
 public:
  using LWF::Update<Innovation,FilterState,UpdateMeas,UpdateNoise,OutlierDetectionExample,PredictionExample,true>::eval;
  using mtState = LWF::Update<Innovation,FilterState,UpdateMeas,UpdateNoise,OutlierDetectionExample,PredictionExample,true>::mtState;
  typedef UpdateMeas mtMeas;
  typedef UpdateNoise mtNoise;
  typedef Innovation mtInnovation;
  PredictAndUpdateExample(){};
  ~PredictAndUpdateExample(){};
  void eval(mtInnovation& inn, const mtState& state, const mtMeas& meas, const mtNoise noise, double dt = 0.0) const{
    inn.get<Innovation::POS>() = state.get<State::POS>()-meas.get<UpdateMeas::POS>()+noise.get<UpdateNoise::POS>();
    inn.get<Innovation::HEI>() = Eigen::Vector3d(0,0,1).dot(state.get<State::POS>())-meas.get<UpdateMeas::HEI>()+noise.get<UpdateNoise::HEI>();;
  }
  void jacInput(mtJacInput& J, const mtState& state, const mtMeas& meas, double dt = 0.0) const{
    mtInnovation inn;
    J.setZero();
    J.template block<3,3>(mtInnovation::getId<Innovation::POS>(),mtState::getId<State::POS>()) = Eigen::Matrix3d::Identity();
    J.template block<1,3>(mtInnovation::getId<Innovation::HEI>(),mtState::getId<State::POS>()) = Eigen::Vector3d(0,0,1).transpose();
  }
  void jacNoise(mtJacNoise& J, const mtState& state, const mtMeas& meas, double dt = 0.0) const{
    mtInnovation inn;
    J.setZero();
    J.template block<3,3>(mtInnovation::getId<Innovation::POS>(),mtNoise::getId<mtNoise::POS>()) = Eigen::Matrix3d::Identity();
    J(mtInnovation::getId<Innovation::HEI>(),mtNoise::getId<mtNoise::HEI>()) = 1.0;
  }
};

}

class NonlinearTest{
 public:
  static const int id_ = 0;
  typedef Nonlinear::State mtState;
  typedef Nonlinear::FilterState mtFilterState;
  typedef Nonlinear::UpdateMeas mtUpdateMeas;
  typedef Nonlinear::UpdateNoise mtUpdateNoise;
  typedef Nonlinear::Innovation mtInnovation;
  typedef Nonlinear::PredictionNoise mtPredictionNoise;
  typedef Nonlinear::PredictionMeas mtPredictionMeas;
  typedef Nonlinear::UpdateExample mtUpdateExample;
  typedef Nonlinear::PredictionExample mtPredictionExample;
  typedef Nonlinear::PredictAndUpdateExample mtPredictAndUpdateExample;
  typedef Nonlinear::OutlierDetectionExample mtOutlierDetectionExample;
  void init(mtState& state,mtUpdateMeas& updateMeas,mtPredictionMeas& predictionMeas){
    state.get<mtState::POS>() = Eigen::Vector3d(2.1,-0.2,-1.9);
    state.get<mtState::VEL>() = Eigen::Vector3d(0.3,10.9,2.3);
    state.get<mtState::ACB>() = Eigen::Vector3d(0.3,10.9,2.3);
    state.get<mtState::GYB>() = Eigen::Vector3d(0.3,10.9,2.3);
    state.get<mtState::ATT>() = rot::RotationQuaternionPD(4.0/sqrt(30.0),3.0/sqrt(30.0),1.0/sqrt(30.0),2.0/sqrt(30.0));
    updateMeas.get<mtUpdateMeas::POS>() = Eigen::Vector3d(-1.5,12,5.23);
    updateMeas.get<mtUpdateMeas::ATT>() = rot::RotationQuaternionPD(3.0/sqrt(15.0),-1.0/sqrt(15.0),1.0/sqrt(15.0),2.0/sqrt(15.0));
    predictionMeas.get<mtPredictionMeas::ACC>() = Eigen::Vector3d(-5,2,17.3);
    predictionMeas.get<mtPredictionMeas::GYR>() = Eigen::Vector3d(15.7,0.45,-2.3);
  }
};

class LinearTest{
 public:
  static const int id_ = 1;
  typedef Linear::State mtState;
  typedef Linear::FilterState mtFilterState;
  typedef Linear::UpdateMeas mtUpdateMeas;
  typedef Linear::UpdateNoise mtUpdateNoise;
  typedef Linear::Innovation mtInnovation;
  typedef Linear::PredictionNoise mtPredictionNoise;
  typedef Linear::PredictionMeas mtPredictionMeas;
  typedef Linear::UpdateExample mtUpdateExample;
  typedef Linear::PredictionExample mtPredictionExample;
  typedef Linear::PredictAndUpdateExample mtPredictAndUpdateExample;
  typedef Linear::OutlierDetectionExample mtOutlierDetectionExample;
  void init(mtState& state,mtUpdateMeas& updateMeas,mtPredictionMeas& predictionMeas){
    state.get<mtState::POS>() = Eigen::Vector3d(2.1,-0.2,-1.9);
    state.get<mtState::VEL>() = Eigen::Vector3d(0.3,10.9,2.3);
    updateMeas.get<mtUpdateMeas::POS>() = Eigen::Vector3d(-1.5,12,5.23);
    updateMeas.get<mtUpdateMeas::HEI>() = 0.5;
    predictionMeas.get<mtPredictionMeas::ACC>() = Eigen::Vector3d(-5,2,17.3);
  }
};

}

#endif /* TestClasses_HPP_ */
