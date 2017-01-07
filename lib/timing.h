#include <string>
#include <vector>

using std::vector;
using std::pair;
using std::string;

#ifndef TIMER
#define TIMER

class Timer {
private:
    struct timeStamp {
        string name;
        timeval time;

        timeStamp(string name) {
            this->name = name;
            this->time = timeval();
        }

        timeStamp(string name, timeval time) {
            this->name = name;
            this->time = time;
        }
    } startTime{"Start"}, endTime{"End"};

    vector<timeStamp> checkPoints;

    void outputDiff(timeStamp t1, timeStamp t2);

public:
    Timer();

    void start();

    void checkPoint(std::string name);

    void stop();
};

#endif