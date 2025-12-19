#ifndef __CONTROLLER_HPP__
#define __CONTROLLER_HPP__

#include <thread>
#include <mutex>
#include <functional>

class Gui;
class Epayment;
class WorkflowManager;

class Controller
{
private:
    bool isRun;
    Epayment &epayment;
    WorkflowManager &workflow;
    Gui &gui;
    std::thread *th;
    mutable std::mutex mtx;

    bool processAttachedCard();

    void routine();

public:
    Controller(Epayment &epayment, WorkflowManager &workflow, Gui &gui);
    ~Controller();

    void setup(std::function<void(Epayment &epayment, WorkflowManager &workflow, Gui &gui)> handler);

    bool isRuning();

    void begin(std::function<void(Epayment &epayment, WorkflowManager &workflow, Gui &gui)> preSetup);
    void stop();
};

#endif