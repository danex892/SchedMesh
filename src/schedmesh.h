#ifndef SCHEDMESH_H
#define SCHEDMESH_H

#include <algorithm>
#include <iostream>
#include <iterator>
#include <cstring>
#include <fstream>
#include <sstream>
#include <random>
#include <string>
#include <thread>
#include <vector>
#include <ctime>
#include <chrono>
#include <mutex>
#include <map>
#include <array>

using namespace std;

typedef mt19937 MyRNG; // the Mersenne Twister with a popular choice of parameters

struct Lesson
{
    int teacherid;
    int teacherid2;
    int classroom;
    int classroom2;
    string name;
    string name2;
    string groupname;
};

struct GymLesson
{
    string group1name;
    string group2name;
    int group1course;
    int group2course;
};

struct Subject
{
    string name;
    int teacherid;
    int teacherid2;
    int countlessons;
    int profilegroup;
    bool isphysicalculture;
    //bool isdouble;
    bool issubgroups;
    int day[6];
};

struct Group
{
    string name;
    int course;
    string identifier;
    int countsubjects;
    int countlessonsweek;
    int session;
    bool doublelessons;
    bool timeslot[6][14];
    Lesson *lesson[6][14];
    Subject subject[30];
};

struct Teacher
{
    string name;
    int hours;
    int totalwinds;
    int winds[6];
    int classroomslist[20];
    int countrooms;
    bool availability[6][14];
    bool methodday[6];
    Lesson *lesson[6][14];
};

struct Classroom
{
    bool availability[6][14];
    Lesson *lesson[6][14];
};

struct Gym
{
    bool availability[6][14];
    GymLesson lesson[6][14];
};

struct Settings
{
    bool debug;
    bool debug_fstpl;
    bool checkday;
    int bugday;
    int bugclass;

    int threads;
    int days;
    int maxstep;
    int randtype;
    bool random_seed;
    bool last_day_short;
    bool improve_timetable;
    string fname;
    string methodicald_fname;
    string classrooms_fname;
    string output_fname;

    int countclasses;
    int countteachers;
    int maxlessons;
    int sessions;
    int currsession;

    int maxwinds;
    int maxwindsday;
    int totalwinds;

    bool reset_days;
    int errors_limit;

    bool tofile;

    string physicalcultname;
    string debug_fname;

    clock_t begin_t;
    clock_t end_t;
};

class SchedMeshApp
{
public:
    int run();
};

extern Lesson EmptyLessonStruct;
extern MyRNG rng;
extern default_random_engine rng_stand;

extern vector<string> daynames;
extern vector<string> doubles;
extern vector<string> entirecourseinday;
extern vector<string> notfirstlast;
extern map<string,string> conflicts;

bool swap_lessons(vector<Group> &g, vector<Teacher> &t, vector<Classroom> &rooms, Gym &gym, Settings &stn,
                  int c1, int d1, int l1, int d2, int l2, int type);
bool free_slot_then_put_lesson(vector<Group> &g, vector<Teacher> &t, vector<Classroom> &rooms, Gym &gym, Settings &stn,
                               int c, int d1, int l1, int &l_res);
void free_lesson(Group &g, vector<Teacher> &t, vector<Classroom> &rooms, Gym &gym,
                 int t1, int t2, int r1, int r2, int d, int l, int s, int s1);
void put_lesson(Group &g, vector<Teacher> &t, vector<Classroom> &rooms, Gym &gym,
                int t1, int t2, int r1, int r2, int d, int l, int s, int s1);
void generate_timetable(vector<Group> classes, vector<Teacher> teachers, vector<Classroom> rooms, Gym *gym, Settings stn);

#endif // SCHEDMESH_H
