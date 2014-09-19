#include <ipa_canopen_master/canopen.h>
#include <ipa_canopen_master/master.h>
#include <boost/make_shared.hpp>
#include <iostream>

#include <ipa_can_interface/dispatcher.h>
#include <boost/unordered_set.hpp>
#include <ipa_can_interface/socketcan.h>

#include <boost/thread.hpp>

#include <ipa_canopen_402/ipa_canopen_402.h>

#include <signal.h>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>

bool running = true;

void my_handler(int s){
           printf("Caught signal %d\n",s);
           running = false;
}

using namespace ipa_can;
using namespace ipa_canopen;

boost::shared_ptr<ThreadedInterface<SocketCANInterface> > driver = boost::make_shared<ThreadedInterface<SocketCANInterface> > (true);

void print_frame(const Frame &f){
  LOG( "in: " << std:: hex << f.id << std::dec);
}
void print_tpdo(const Frame &f){
  LOG( "TPDO: " << std:: hex << f.id << std::dec);
}

void print_state(const State &f){
  LOG("STATE");
}

void print_node_state(const Node::State &s){
  LOG("NMT:" << s);
}

int main(int argc, char *argv[])
{

  if(argc < 3){
    std::cout << "Usage: " << argv[0] << " DEVICE EDS/DCF [sync_ms]" << std::endl;
    return -1;
  }

  struct sigaction sigIntHandler;

  sigIntHandler.sa_handler = my_handler;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;

  sigaction(SIGINT, &sigIntHandler, NULL);

  // Interface::FrameListener::Ptr printer = driver->createMsgListener(print_frame); // printer for all incoming messages
  // Interface::FrameListener::Ptr tprinter = driver->createMsgListener(Header(0x181), print_tpdo); // printer for all incoming messages
  StateInterface::StateListener::Ptr sprinter = driver->createStateListener(print_state); // printer for all incoming messages

  int sync_ms = 10;
  if(argc > 3) sync_ms = atol(argv[3]);

//  if(!driver->init(argv[1],0)){
//    std::cout << "init failed" << std::endl;
//    return -1;
//  }

  sleep(1.0);

  boost::shared_ptr<ipa_canopen::ObjectDict>  dict = ipa_canopen::ObjectDict::fromFile(argv[2]);

  LocalMaster master(argv[1], driver);
  boost::shared_ptr<SyncLayer> sync = master.getSync(SyncProperties(Header(0x80), boost::posix_time::milliseconds(sync_ms), 0));

  boost::shared_ptr<ipa_canopen::Node> node (new Node(driver, dict, 85, sync));

  std::string name = "402";
  boost::shared_ptr<Node_402> motor( new Node_402(node, name));


  LayerStack stack("test");
  stack.add(boost::make_shared<CANLayer<ThreadedSocketCANInterface > >(driver, argv[1], 0));
  stack.add(sync);
  stack.add(node);
  stack.add(motor);
  LayerStatusExtended es;

  stack.init(es);
  LayerStatus s;

  if(sync){
      LayerStatus r,w;
      sync->read(r);
      sync->write(w);
  }

  bool flag_op = false;
  int count = 0;

  while(running)
  {
    LayerStatus r,w;
    stack.read(r);
    stack.write(w);
    boost::this_thread::interruption_point();
    if(count > 500)
    {
      count = 0;
      flag_op = !flag_op;
    }
    if(flag_op)
    {
      LOG("Current mode:" << (int)motor->getMode() << " Count: " << count << "Current Pos: " << motor->getActualPos());
      motor->enterMode(motor->Profiled_Velocity);
      motor->setTargetVel(-360000);
    }
    else
    {
      LOG("Current mode:" << (int)motor->getMode() << " Count: " << count << "Current Pos: " << motor->getActualPos());
      motor->setTargetPos(1000);
      motor->enterMode(motor->Profiled_Position);
    }
    count++;
  }

  motor->turnOff();

  stack.shutdown(s);

  return 0;
}

