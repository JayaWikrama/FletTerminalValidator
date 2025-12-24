#ifndef __CONTROLLER_HPP__
#define __CONTROLLER_HPP__

#include <thread>
#include <mutex>
#include <functional>
#include <memory>

class Gui;
class Epayment;
class WorkflowManager;
class Duration;

class Controller
{
private:
    bool isRun;
    Epayment &epayment;
    WorkflowManager &workflow;
    Gui &gui;
    std::unique_ptr<std::thread> th;
    mutable std::mutex mtx;

    bool processAttachedCard(Duration &duration);

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