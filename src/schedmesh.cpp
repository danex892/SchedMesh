#include "schedmesh.h"
#include "auxiliary.h"
#include "printdata.h"
#include "csvrow.h"
#include <limits>

#define global_maxlessons 14

mutex m;

int mintotalwinds = 1000000000;
int minmaxwinds = 1000000000;
int minmaxwindsday = 1000000000;

int step = 0;
int bugstep = 0;
int brokendays = 0;

vector<Group> bestclasses(50);
vector<Teacher> bestteachers(100);
vector<Classroom> bestrooms(50);
Gym* bestgym = new Gym();
Settings beststn;

array<vector<Teacher>, 200> resteachers;
array<vector<Group>, 200> resclasses;
array<vector<Classroom>, 200> resrooms;
vector<Gym> resgym(200);

int res_ind = 0;

Lesson EmptyLessonStruct = {0, 0, 0, 0, "", "", ""};

random_device rd{};
MyRNG rng(rd());

default_random_engine rng_stand;

vector<string> daynames;
vector<string> doubles;
vector<string> entirecourseinday;
vector<string> notfirstlast;
map<string,string> conflicts;

namespace
{
void reset_runtime_state()
{
    mintotalwinds = 1000000000;
    minmaxwinds = 1000000000;
    minmaxwindsday = 1000000000;

    step = 0;
    bugstep = 0;
    brokendays = 0;
    res_ind = 0;

    bestclasses.assign(50, Group{});
    bestteachers.assign(100, Teacher{});
    bestrooms.assign(50, Classroom{});
    *bestgym = Gym{};
    beststn = Settings{};

    for (auto &teachers : resteachers) teachers.clear();
    for (auto &classes : resclasses) classes.clear();
    for (auto &rooms : resrooms) rooms.clear();
    resgym.assign(200, Gym{});
}
}

bool swap_lessons(vector<Group> &g, vector<Teacher> &t, vector<Classroom> &rooms, Gym &gym, Settings &stn,
                  int c1, int d1, int l1, int d2, int l2, int type)
{
    bool res = false;

    int r[8];
    fill(r, r + 8, 0);
    int prob_tid1, prob_tid2, prob_tid1s2, prob_tid2s2;
    prob_tid1 = prob_tid2 = prob_tid1s2 = prob_tid2s2 = 0;

    int tid1 = g[c1].lesson[d1][l1]->teacherid;
    int tid2 = g[c1].lesson[d2][l2]->teacherid;

    int tid1s2 = g[c1].lesson[d1][l1]->teacherid2;
    int tid2s2 = g[c1].lesson[d2][l2]->teacherid2;
    if (tid1 != tid2 && t[tid1].availability[d2][l2] && t[tid2].availability[d1][l1]
            && t[tid1s2].availability[d2][l2] && t[tid2s2].availability[d1][l1]
            && !t[tid1].methodday[d2] && !t[tid2].methodday[d1]
            && !t[tid1s2].methodday[d2] && !t[tid2s2].methodday[d1]
            && check_room(rooms, t, tid1, d2, l2, r[0], r[1], prob_tid1, r, 0)
            && check_room(rooms, t, tid1s2, d2, l2, r[2], r[3], prob_tid1s2, r, 2)
            && check_room(rooms, t, tid2, d1, l1, r[4], r[5], prob_tid2, r, 4)
            && check_room(rooms, t, tid2s2, d1, l1, r[6], r[7], prob_tid2s2, r, 6))
    {
        string l1name = g[c1].lesson[d1][l1]->name;
        string l2name = g[c1].lesson[d2][l2]->name;
        string l1name2 = g[c1].lesson[d1][l1]->name2;
        string l2name2 = g[c1].lesson[d2][l2]->name2;

        int l1num, l1num2, l2num, l2num2;
        l1num = l1num2 = l2num = l2num2 = 0;
        get_subject_num(g[c1], tid1, tid1s2, l1name, l1name2, l1num, l1num2);
        get_subject_num(g[c1], tid2, tid2s2, l2name, l2name2, l2num, l2num2);

        if ((g[c1].subject[l2num].day[d1] == 0 || g[c1].doublelessons)
                && (g[c1].subject[l1num].day[d2] == 0 || g[c1].doublelessons)
                && check_physical_culture(g[c1], gym, l1num, d2, l2)
                && check_physical_culture(g[c1], gym, l2num, d1, l1)
                && !is_double(g[c1].subject[l1num].name)
                && !is_double(g[c1].subject[l2num].name)
                && check_first_last(g[c1], l1num, l2, stn)
                && check_first_last(g[c1], l2num, l1, stn)
                && check_conflicts(g[c1], l1num, d2, stn.maxlessons)
                && check_conflicts(g[c1], l2num, d1, stn.maxlessons))
        {
            int w1_d1, w1_d2, w2_d1, w2_d2;
            int nw1_d1, nw1_d2, nw2_d1, nw2_d2;
            int w1s2_d1, w1s2_d2, w2s2_d1, w2s2_d2;
            int nw1s2_d1, nw1s2_d2, nw2s2_d1, nw2s2_d2;
            int tw1, tw1s2, tw2, tw2s2;

            w1_d1 = w1_d2 = w2_d1 = w2_d2 = 0;
            nw1_d1 = nw1_d2 = nw2_d1 = nw2_d2 = 0;
            w1s2_d1 = w1s2_d2 = w2s2_d1 = w2s2_d2 = 0;
            nw1s2_d1 = nw1s2_d2 = nw2s2_d1 = nw2s2_d2 = 0;
            tw1 = tw1s2 = tw2 = tw2s2 = 0;

            if (type == 1)
            {
                w1_d1 = t[tid1].winds[d1];
                w1_d2 = t[tid1].winds[d2];
                w2_d1 = t[tid2].winds[d1];
                w2_d2 = t[tid2].winds[d2];

                t[tid1].availability[d2][l2] = false;
                t[tid2].availability[d1][l1] = false;

                t[tid1].availability[d1][l1] = true;
                t[tid2].availability[d2][l2] = true;

                nw1_d1 = calc_winds(t[tid1], d1, stn.maxlessons);
                nw1_d2 = calc_winds(t[tid1], d2, stn.maxlessons);
                nw2_d1 = calc_winds(t[tid2], d1, stn.maxlessons);
                nw2_d2 = calc_winds(t[tid2], d2, stn.maxlessons);

                t[tid1].availability[d2][l2] = true;
                t[tid2].availability[d1][l1] = true;

                t[tid1].availability[d1][l1] = false;
                t[tid2].availability[d2][l2] = false;

                tw1 = t[tid1].totalwinds - w1_d1 + nw1_d1 - w1_d2 + nw1_d2;
                tw2 = t[tid2].totalwinds - w2_d1 + nw2_d1 - w2_d2 + nw2_d2;

                if (tid1s2 != 99)
                {
                    w1s2_d1 = t[tid1s2].winds[d1];
                    w1s2_d2 = t[tid1s2].winds[d2];

                    t[tid1s2].availability[d2][l2] = false;
                    t[tid1s2].availability[d1][l1] = true;

                    nw1s2_d1 = calc_winds(t[tid1s2], d1, stn.maxlessons);
                    nw1s2_d2 = calc_winds(t[tid1s2], d2, stn.maxlessons);

                    t[tid1s2].availability[d2][l2] = true;
                    t[tid1s2].availability[d1][l1] = false;

                    tw1s2 = t[tid1s2].totalwinds - w1s2_d1 + nw1s2_d1 - w1s2_d2 + nw1s2_d2;
                }
                if (tid2s2 != 99)
                {
                    w2s2_d1 = t[tid2s2].winds[d1];
                    w2s2_d2 = t[tid2s2].winds[d2];

                    t[tid2s2].availability[d1][l1] = false;
                    t[tid2s2].availability[d2][l2] = true;

                    nw2s2_d1 = calc_winds(t[tid2s2], d1, stn.maxlessons);
                    nw2s2_d2 = calc_winds(t[tid2s2], d2, stn.maxlessons);

                    t[tid2s2].availability[d1][l1] = true;
                    t[tid2s2].availability[d2][l2] = false;

                    tw2s2 = t[tid2s2].totalwinds - w2s2_d1 + nw2s2_d1 - w2s2_d2 + nw2s2_d2;
                }
            }

            if (type == 0 ||
               (type == 1 &&
                    (tw1 <= t[tid1].totalwinds && tw2 <= t[tid2].totalwinds && tw1s2 <= t[tid1s2].totalwinds && tw2s2 <= t[tid2s2].totalwinds)
                    && (tw1 < t[tid1].totalwinds || tw2 < t[tid2].totalwinds || tw1s2 < t[tid1s2].totalwinds || tw2s2 < t[tid2s2].totalwinds))
               )
            {
                t[tid1].totalwinds = tw1;
                t[tid2].totalwinds = tw2;
                t[tid1s2].totalwinds = tw1s2;
                t[tid2s2].totalwinds = tw2s2;

                t[tid1].winds[d1] = nw1_d1;
                t[tid1].winds[d2] = nw1_d2;
                t[tid1s2].winds[d1] = nw1s2_d1;
                t[tid1s2].winds[d2] = nw1s2_d2;
                t[tid2].winds[d1] = nw2_d1;
                t[tid2].winds[d2] = nw2_d2;
                t[tid2s2].winds[d1] = nw2s2_d1;
                t[tid2s2].winds[d2] = nw2s2_d2;

                move_teacher_from_to_room(rooms, d2, l2, prob_tid1, r[0], r[1]);
                move_teacher_from_to_room(rooms, d2, l2, prob_tid1s2, r[2], r[3]);
                move_teacher_from_to_room(rooms, d1, l1, prob_tid2, r[4], r[5]);
                move_teacher_from_to_room(rooms, d1, l1, prob_tid2s2, r[6], r[7]);

                int r1_old = get_classroom_of_teacher(t[tid1], tid1, d1, l1);
                int r2_old = get_classroom_of_teacher(t[tid1s2], tid1s2, d1, l1);
                int r3_old = get_classroom_of_teacher(t[tid2], tid2, d2, l2);
                int r4_old = get_classroom_of_teacher(t[tid2s2], tid2s2, d2, l2);

                free_lesson(g[c1], t, rooms, gym, tid1, tid1s2, r1_old, r2_old, d1, l1, l1num, l1num2);
                free_lesson(g[c1], t, rooms, gym, tid2, tid2s2, r3_old, r4_old, d2, l2, l2num, l2num2);

                put_lesson(g[c1], t, rooms, gym, tid1, tid1s2, r[0], r[2], d2, l2, l1num, l1num2);
                put_lesson(g[c1], t, rooms, gym, tid2, tid2s2, r[4], r[6], d1, l1, l2num, l2num2);

                res = true;
            }
        }
    }
    return res;
}

bool free_slot_then_put_lesson(vector<Group> &g, vector<Teacher> &t, vector<Classroom> &rooms, Gym &gym, Settings &stn,
                               int c, int d1, int l1, int &l_res)
{
    bool result = false;
    int r[8];
    int prob_tid1, prob_tid2, prob_tid1s2, prob_tid2s2;
    for (int l1num = 0; l1num < g[c].countsubjects; l1num++)
    {
        if (result) break;
        if (is_double(g[c].subject[l1num].name))
        {
            continue;
        }

        if (g[c].subject[l1num].profilegroup == 2)
        {
            continue;
        }

        if (g[c].subject[l1num].countlessons > 0)
        {
            int tid1 = g[c].subject[l1num].teacherid;
            int tid1s2 = g[c].subject[l1num].teacherid2;
            string l1name = g[c].subject[l1num].name;

            fill(r, r + 8, 0);
            prob_tid1 = prob_tid2 = prob_tid1s2 = prob_tid2s2 = 0;
            if (g[c].timeslot[d1][l1] &&
                    !g[c].subject[l1num].issubgroups &&
                    g[c].subject[l1num].profilegroup == 0 &&
                    !t[tid1].methodday[d1] &&
                    !t[tid1s2].methodday[d1] &&
                    (g[c].subject[l1num].day[d1] == 0 || g[c].doublelessons) &&
                    check_physical_culture(g[c], gym, l1num, d1, l1) &&
                    check_first_last(g[c], l1num, l1, stn) &&
                    check_conflicts(g[c], l1num, d1, l1) &&
                    check_room(rooms, t, tid1, d1, l1, r[0], r[1], prob_tid1, r, 0) &&
                    check_room(rooms, t, tid1s2, d1, l1, r[2], r[3], prob_tid1s2, r, 2))
            {
                string l1groupname = t[tid1].lesson[d1][l1]->groupname;
                int c1 = 0;
                for (c1 = 0; c1 < stn.countclasses; c1++)
                {
                    if (result) break;
                    if (g[c1].name == l1groupname)
                    {
                        break;
                    }
                }

                for (int d2 = 0; d2 <= d1; d2++)
                {
                    if (result) break;
                    for (int l2 = 0; l2 < stn.maxlessons; l2++)
                    {
                        if (g[c1].timeslot[d2][l2])
                        {
                            if (swap_lessons(g, t, rooms, gym, stn, c1, d1, l1, d2, l2, 0))
                            {
                                put_lesson(g[c], t, rooms, gym, tid1, tid1s2, r[0], r[2], d1, l1, l1num, -1);

                                l_res = l1num;

                                result = true;
                                break;
                            }
                        }
                    }
                }
            }
            if (result) break;

            for (int d2 = 0; d2 <= d1; d2++)
            {
                if (result) break;
                if (t[tid1].methodday[d2]) continue;
                if (t[tid1s2].methodday[d2]) continue;

                bool check_day = true;
                if (!g[c].doublelessons)
                {
                    for (int l2 = 0; l2 < stn.maxlessons; l2++)
                    {
                        if (g[c].lesson[d2][l2]->name == l1name)
                        {
                            check_day = false;
                            break;
                        }
                    }
                }

                if (!check_day) continue;

                for (int l2 = 0; l2 < stn.maxlessons; l2++)
                {
                    if (d2 == d1 && l2 == l1)
                    {
                        break;
                    }

                    fill(r, r + 8, 0);
                    prob_tid1 = prob_tid2 = prob_tid1s2 = prob_tid2s2 = 0;
                    int l1num2 = -1;

                    if (!t[tid1].availability[d2][l2] ||
                        !t[tid1s2].availability[d2][l2] ||
                        !g[c].timeslot[d2][l2] ||
                        !check_physical_culture(g[c], gym, l1num, d2, l2) ||
                        !check_first_last(g[c], l1num, l2, stn) ||
                        !check_conflicts(g[c], l1num, d2, stn.maxlessons) ||
                        !check_profile_lesson(g[c], t, tid1, tid1s2, l1num, l1num2, d2, l2, stn.randtype) ||
                        !check_room(rooms, t, tid1, d2, l2, r[0], r[1], prob_tid1, r, 0) ||
                        !check_room(rooms, t, tid1s2, d2, l2, r[2], r[3], prob_tid1s2, r, 2))
                    {
                        continue;
                    }

                    int tid2 = g[c].lesson[d2][l2]->teacherid;
                    int tid2s2 = g[c].lesson[d2][l2]->teacherid2;
                    if (t[tid2].availability[d1][l1] &&
                            t[tid2s2].availability[d1][l1] &&
                            !t[tid2].methodday[d1] &&
                            !t[tid2s2].methodday[d1] &&
                            check_room(rooms, t, tid2, d1, l1, r[4], r[5], prob_tid2, r, 4) &&
                            check_room(rooms, t, tid2s2, d1, l1, r[6], r[7], prob_tid2s2, r, 6))
                    {
                        string l2name = g[c].lesson[d2][l2]->name;
                        string l2name2 = g[c].lesson[d2][l2]->name2;
                        int l2num, l2num2;
                        l2num = l2num2 = 0;
                        get_subject_num(g[c], tid2, tid2s2, l2name, l2name2, l2num, l2num2);
                        if ((g[c].subject[l2num].day[d1] != 0 && !g[c].doublelessons)
                                || is_double(l2name)
                                || !check_first_last(g[c], l2num, l1, stn)
                                || !check_conflicts(g[c], l2num, d1, stn.maxlessons)
                                || !check_physical_culture(g[c], gym, l2num, d1, l1))
                        {
                            continue;
                        }

                        move_teacher_from_to_room(rooms, d2, l2, prob_tid1, r[0], r[1]);
                        move_teacher_from_to_room(rooms, d2, l2, prob_tid1s2, r[2], r[3]);
                        move_teacher_from_to_room(rooms, d1, l1, prob_tid2, r[4], r[5]);
                        move_teacher_from_to_room(rooms, d1, l1, prob_tid2s2, r[6], r[7]);

                        int r3_old = get_classroom_of_teacher(t[tid2], tid2, d2, l2);
                        int r4_old = get_classroom_of_teacher(t[tid2s2], tid2s2, d2, l2);

                        put_lesson(g[c], t, rooms, gym, tid2, tid2s2, r[4], r[6], d1, l1, l2num, l2num2);
                        free_lesson(g[c], t, rooms, gym, tid2, tid2s2, r3_old, r4_old, d2, l2, l2num, l2num2);

                        put_lesson(g[c], t, rooms, gym, tid1, tid1s2, r[0], r[2], d2, l2, l1num, l1num2);

                        l_res = l1num;

                        result = true;
                        break;
                    }
                }
            }
        }
    }
    return result;
}

void free_lesson(Group &g, vector<Teacher> &t, vector<Classroom> &rooms, Gym &gym,
                 int t1, int t2, int r1, int r2, int d, int l, int s, int s1)
{
    if (g.subject[s].isphysicalculture)
    {
        if (gym.lesson[d][l].group1course == g.course
                && gym.lesson[d][l].group1name == g.name)
        {
            if (gym.lesson[d][l].group2course != 0)
            {
                gym.lesson[d][l].group1course = gym.lesson[d][l].group2course;
                gym.lesson[d][l].group1name = gym.lesson[d][l].group2name;
                gym.lesson[d][l].group2course = 0;
                gym.lesson[d][l].group2name = "";
                gym.availability[d][l] = true;
            }
            else
            {
                gym.lesson[d][l].group1course = 0;
                gym.lesson[d][l].group1name = "";
            }

        }
        else if (gym.lesson[d][l].group2course == g.course
                 && gym.lesson[d][l].group2name == g.name)
        {
            gym.lesson[d][l].group2course = 0;
            gym.lesson[d][l].group2name = "";
            gym.availability[d][l] = true;
        }
    }

    rooms[r1].availability[d][l] = true;
    rooms[r1].lesson[d][l] = &EmptyLessonStruct;

    t[t1].availability[d][l] = true;
    t[t1].lesson[d][l] = &EmptyLessonStruct;
    g.lesson[d][l] = &EmptyLessonStruct;
    g.subject[s].day[d]--;
    if (s1 != -1)
    {
        g.subject[s1].day[d]--;
    }

    if (g.subject[s].issubgroups || s1 != -1)
    {
        t[t2].availability[d][l] = true;
        t[t2].lesson[d][l] = &EmptyLessonStruct;

        rooms[r2].availability[d][l] = true;
        rooms[r2].lesson[d][l] = &EmptyLessonStruct;
    }

    g.subject[s].countlessons++;
    g.countlessonsweek++;
    if (s1 != -1)
    {
        g.subject[s1].countlessons++;
    }
}

void put_lesson(Group &g, vector<Teacher> &t, vector<Classroom> &rooms, Gym &gym,
                int t1, int t2, int r1, int r2, int d, int l, int s, int s1)
{
    Lesson* currlesson = new Lesson();

    currlesson->classroom = r1;
    currlesson->classroom2 = r2;
    currlesson->teacherid = t1;
    currlesson->teacherid2 = t2;
    currlesson->name = g.subject[s].name;
    if (s1 != -1)
    {
        currlesson->name2 = g.subject[s1].name;
    }
    currlesson->groupname = g.name;

    if (g.subject[s].isphysicalculture)
    {
        if (gym.availability[d][l])
        {
            if (gym.lesson[d][l].group1course == 0)
            {
                gym.lesson[d][l].group1course = g.course;
                gym.lesson[d][l].group1name = g.name;
            }
            else
            {
                if (gym.lesson[d][l].group1course == g.course
                    || gym.lesson[d][l].group1course == g.course + 1
                    || gym.lesson[d][l].group1course == g.course - 1)
                {
                    gym.lesson[d][l].group2course = g.course;
                    gym.lesson[d][l].group2name = g.name;
                    gym.availability[d][l] = false;
                }
            }
        }
    }

    t[t1].availability[d][l] = false;
    if (r1 != 0)
    {
        rooms[r1].availability[d][l] = false;
        rooms[r1].lesson[d][l] = currlesson;
    }
    t[t1].lesson[d][l] = currlesson;
    g.lesson[d][l] = currlesson;
    g.subject[s].countlessons--;
    g.subject[s].day[d]++;
    g.countlessonsweek--;
    if (s1 != -1)
    {
        g.subject[s1].countlessons--;
        g.subject[s1].day[d]++;
    }

    if (g.subject[s].issubgroups || s1 != -1)
    {
        t[t2].availability[d][l] = false;
        if (r2 != 0)
        {
            rooms[r2].availability[d][l] = false;
            rooms[r2].lesson[d][l] = currlesson;
        }
        t[t2].lesson[d][l] = currlesson;
    }
}

int calc_subject_constraint_score(Group &g, vector<Teacher> &t, int subj, int tid1, int tid1s2, Settings &stn)
{
    int available_week_slots = 0;
    for (int dd = 0; dd < stn.days; dd++)
    {
        if (t[tid1].methodday[dd] || t[tid1s2].methodday[dd])
        {
            continue;
        }
        for (int ll = 0; ll < stn.maxlessons; ll++)
        {
            if (g.timeslot[dd][ll] && t[tid1].availability[dd][ll] && t[tid1s2].availability[dd][ll])
            {
                available_week_slots++;
            }
        }
    }

    // Smaller score = more constrained subject => schedule first.
    return available_week_slots * 10 + g.subject[subj].countlessons;
}

int pick_most_constrained_subject_position(
    Group &g,
    vector<Teacher> &teachers,
    vector<Classroom> &rooms,
    Gym &gym,
    Settings &stn,
    array<int, 100> &subjectInds,
    int fromIndex,
    int toIndex,
    int d,
    int l)
{
    int best_pos = -1;
    int best_score = std::numeric_limits<int>::max();

    for (int p = fromIndex; p < toIndex; p++)
    {
        int t = subjectInds[p];
        int t1 = -1;
        int tid1 = g.subject[t].teacherid;
        int tid1s2 = g.subject[t].teacherid2;
        int prob_tid1 = 0;
        int prob_tid1s2 = 0;
        int r[4] = {0, 0, 0, 0};

        bool feasible =
            !teachers[tid1].methodday[d] &&
            !teachers[tid1s2].methodday[d] &&
            (g.subject[t].day[d] == 0 || g.doublelessons) &&
            teachers[tid1].availability[d][l] &&
            teachers[tid1s2].availability[d][l] &&
            g.subject[t].countlessons > 0 &&
            check_physical_culture(g, gym, t, d, l) &&
            check_double_lesson(g, teachers[tid1], t, d, l) &&
            check_conflicts(g, t, d, l) &&
            check_first_last(g, t, l, stn) &&
            check_profile_lesson(g, teachers, tid1, tid1s2, t, t1, d, l, stn.randtype) &&
            check_room(rooms, teachers, tid1, d, l, r[0], r[1], prob_tid1, r, 0) &&
            check_room(rooms, teachers, tid1s2, d, l, r[2], r[3], prob_tid1s2, r, 2);

        if (!feasible)
        {
            continue;
        }

        int score = calc_subject_constraint_score(g, teachers, t, tid1, tid1s2, stn);
        if (score < best_score)
        {
            best_score = score;
            best_pos = p;
        }
    }

    return best_pos;
}

void generate_timetable(vector<Group> classes, vector<Teacher> teachers, vector<Classroom> rooms, Gym *gym, Settings stn)
{
    vector<Group> classes2(50);
    vector<Teacher> teachers2(100);
    vector<Classroom> rooms2(50);
    Gym* gym2 = new Gym();

    vector<Group> tempclasses(50);
    vector<Teacher> tempteachers(100);
    vector<Classroom> temprooms(50);
    Gym* tempgym = new Gym();

    vector<array<int, 100>> subjectInds(20);
    vector<int> numInds(20);
    vector<int> currInd(20);

    vector<array<int, 100>> tempsubjectInds(20);
    vector<int> tempnumInds(20);

    *gym2 = *gym;
    rooms2 = rooms;
    classes2 = classes;
    teachers2 = teachers;

    if (stn.sessions > 1)
    {
        m.lock();

        cout << "Generate session " << stn.currsession << endl;

        m.unlock();
    }

    int r[4];
    int counterrors = 0;
    int c = 0;
    int d = 0;
    int l = 0;
    int tid1, tid1s2, prob_tid1, prob_tid1s2;
    bool validschedule = true;

    if (stn.random_seed)
    {
        const auto now_seed = chrono::system_clock::now().time_since_epoch().count();
        rng_stand.seed(static_cast<default_random_engine::result_type>(now_seed));
    }
    else
    {
        rng_stand.seed(0);
    }

    int step_new = 0;
    if (stn.currsession == 1) step_new = 5;
    else step_new = stn.maxstep;
    while (step < step_new)
    {
        int i = 1;
        for (vector<Group>::iterator g = classes.begin(); g != classes.end(); ++g)
        {
            int t = g->countlessonsweek;
            int A[10];
            if (t % stn.days == 0)
            {
                int num = t / stn.days;
                for (int p = 0; p < stn.days; p++)
                {
                    A[p] = num;
                }
            }
            else
            {
                int min = t / stn.days;
                int max = min + 1;
                for (int j = 0; j < stn.days; j++)
                {
                    A[j] = max;
                    t -= max;
                    if (t % min == 0 && t / min == stn.days - j - 1)
                    {
                        for (int p = j + 1; p < stn.days; p++)
                        {
                            A[p] = min;
                        }
                        break;
                    }
                }
                if (stn.randtype == 0)
                {
                    shuffle(&A[0], &A[stn.days - stn.last_day_short], rng_stand);
                }
                else
                {
                    shuffle(&A[0], &A[stn.days - stn.last_day_short], rng);
                }
            }
            for (int j = 0; j < stn.days; j++)
            {
                for (int p = 0; p < A[j]; p++)
                {
                    g->timeslot[j][p] = true;
                }
            }
            if (i == stn.countclasses) break;
            i++;
        }

        fill(subjectInds.begin(), subjectInds.end(), array<int, 100>{0});
        fill(numInds.begin(), numInds.end(), 0);

        counterrors = 0;
        d = 0;
        validschedule = true;
        while (d < stn.days)
        {
            if (stn.reset_days)
            {
                if (!validschedule)
                {
                    counterrors++;
                    m.lock();
                    brokendays++;
                    m.unlock();
                    if (counterrors >= stn.errors_limit)
                    {
                        break;
                    }
                    else
                    {
                        classes = tempclasses;
                        teachers = tempteachers;
                        rooms = temprooms;
                        *gym = *tempgym;

                        subjectInds = tempsubjectInds;
                        numInds = tempnumInds;

                        validschedule = true;
                    }
                }
                else
                {
                    tempclasses = classes;
                    tempteachers = teachers;
                    temprooms = rooms;
                    *tempgym = *gym;

                    tempsubjectInds = subjectInds;
                    tempnumInds = numInds;

                    counterrors = 0;
                }
            }
            else
            {
               if (!validschedule) break;
            }

            c = 0;
            fill(currInd.begin(), currInd.end(), 0);
            for (vector<Group>::iterator g = classes.begin(); g != classes.end(); ++g)
            {
                if (!validschedule) break;
                l = 0;

                if (d == 0)
                {
                    for (int k = 0; k < g->countsubjects; k++)
                    {
                        if (g->subject[k].profilegroup != 2)
                        {
                            for (int p = 0; p < g->subject[k].countlessons; p++)
                            {
                                subjectInds[c][numInds[c]] = k;
                                numInds[c]++;
                            }
                        }
                    }
                    if (stn.randtype == 0)
                    {
                        shuffle(&subjectInds[c][0], &subjectInds[c][numInds[c]], rng_stand);
                    }
                    else
                    {
                        shuffle(&subjectInds[c][0], &subjectInds[c][numInds[c]], rng);
                    }
                }

                while (l < stn.maxlessons)
                {
                    if (g->timeslot[d][l])
                    {
                        int best_pos = pick_most_constrained_subject_position(
                            *g, teachers, rooms, *gym, stn, subjectInds[c], currInd[c], numInds[c], d, l
                        );
                        if (best_pos != -1 && best_pos != currInd[c])
                        {
                            swap(subjectInds[c][best_pos], subjectInds[c][currInd[c]]);
                        }
                    }

                    int t = subjectInds[c][currInd[c]];
                    int t1 = -1;
                    currInd[c]++;

                    if (g->timeslot[d][l] && currInd[c] > numInds[c])
                    {
                        cout << "Something wrong! Current subject index is bigger than maximum index!" << endl;
                    }

                    fill(r, r + 4, 0);
                    prob_tid1 = prob_tid1s2 = 0;
                    tid1 = g->subject[t].teacherid;
                    tid1s2 = g->subject[t].teacherid2;
                    if (g->timeslot[d][l] &&
                            !teachers[tid1].methodday[d] &&
                            !teachers[tid1s2].methodday[d] &&
                            (g->subject[t].day[d] == 0 || g->doublelessons) &&
                            teachers[tid1].availability[d][l] &&
                            teachers[tid1s2].availability[d][l] &&
                            g->subject[t].countlessons > 0 &&
                            check_physical_culture(*g, *gym, t, d, l) &&
                            check_double_lesson(*g, teachers[tid1], t, d, l) &&
                            check_conflicts(*g, t, d, l) &&
                            check_first_last(*g, t, l, stn) &&
                            check_profile_lesson(*g, teachers, tid1, tid1s2, t, t1, d, l, stn.randtype) &&
                            check_room(rooms, teachers, tid1, d, l, r[0], r[1], prob_tid1, r, 0) &&
                            check_room(rooms, teachers, tid1s2, d, l, r[2], r[3], prob_tid1s2, r, 2))
                    {
                        move_teacher_from_to_room(rooms, d, l, prob_tid1, r[0], r[1]);
                        move_teacher_from_to_room(rooms, d, l, prob_tid1s2, r[2], r[3]);

                        put_lesson(*g, teachers, rooms, *gym, tid1, tid1s2, r[0], r[2], d, l, t, t1);

                        l++;

                        subjectInds[c][currInd[c] - 1] = subjectInds[c][numInds[c] - 1];
                        subjectInds[c][numInds[c] - 1] = 0;
                        numInds[c]--;
                        currInd[c] = 0;

                        if (is_double(g->subject[t].name))
                        {
                            put_lesson(*g, teachers, rooms, *gym, tid1, tid1s2, r[0], r[2], d, l, t, t1);

                            bool res1 = false;
                            for (int p = 0; p < numInds[c]; p++)
                            {
                                if (subjectInds[c][p] == t)
                                {
                                    subjectInds[c][p] = subjectInds[c][numInds[c] - 1];
                                    subjectInds[c][numInds[c] - 1] = 0;
                                    numInds[c]--;
                                    res1 = true;
                                    break;
                                }
                            }

                            if (!res1)
                            {
                                cout << "Something wrong! Cannot find " << t
                                     << " after putting double lesson!" << endl;
                            }

                            l++;
                        }
                    }
                    else
                    {
                        if (!g->timeslot[d][l])
                        {
                            currInd[c] = 0;
                            break;
                        }
                        else
                        {
                            if (currInd[c] == numInds[c])
                            {

                                if (stn.debug_fstpl && stn.reset_days && counterrors == stn.errors_limit - 1
                                        && d >= stn.bugday && c >= stn.bugclass)
                                {
                                    print_full_info(classes, teachers, rooms, *gym, stn, step, bugstep, brokendays, 0);
                                }

                                int l1_num;
                                bool res = free_slot_then_put_lesson(classes, teachers, rooms, *gym, stn, c, d, l, l1_num);
                                if (res)
                                {
                                    bool res1 = false;
                                    for (int p = 0; p < numInds[c]; p++)
                                    {
                                        if (subjectInds[c][p] == l1_num)
                                        {
                                            subjectInds[c][p] = subjectInds[c][numInds[c] - 1];
                                            subjectInds[c][numInds[c] - 1] = 0;
                                            numInds[c]--;
                                            currInd[c] = 0;
                                            res1 = true;
                                            break;
                                        }
                                    }

                                    if (!res1)
                                    {
                                        cout << "Something wrong! Cannot find " << l1_num
                                             << " subject after fstpl function!" << endl;
                                    }

                                    l++;
                                }
                                else
                                {
                                    validschedule = false;
                                    d--;
                                    if (stn.debug && !res)
                                    {
                                        if (stn.checkday && (d + 1 < stn.bugday || c < stn.bugclass)) break;
                                        if (stn.reset_days && counterrors != stn.errors_limit - 1) break;

                                        print_full_info(classes, teachers, rooms, *gym, stn, step, bugstep, brokendays, 0);
                                    }
                                    break;
                                }
                            }
                        }
                    }
                }
                if (c + 1 == stn.countclasses) break;
                c++;
            }
            d++;
        }

        if (validschedule)
        {
            if (stn.sessions == 1)
            {
                optimize_timetable(classes, teachers, rooms, *gym, stn);

                m.lock();

                cout << "Step " << step + 1 << endl;

                if (stn.maxwinds <= minmaxwinds)
                {
                    if (stn.maxwinds < minmaxwinds)
                    {
                        minmaxwinds = stn.maxwinds;
                        minmaxwindsday = stn.maxwindsday;
                        mintotalwinds = stn.totalwinds;
                        bestclasses = classes;
                        bestteachers = teachers;
                        bestrooms = rooms;
                        *bestgym = *gym;
                        beststn = stn;
                        cout << "Maximum number of windows on week: " << stn.maxwinds << endl;
                        cout << "Maximum number of windows on day: " << stn.maxwindsday << endl;
                        cout << "Current total windows on week: " << stn.totalwinds << endl;
                    }
                    else if (stn.maxwinds == minmaxwinds && stn.maxwindsday < minmaxwindsday)
                    {
                        mintotalwinds = stn.totalwinds;
                        minmaxwindsday = stn.maxwindsday;
                        bestclasses = classes;
                        bestteachers = teachers;
                        bestrooms = rooms;
                        *bestgym = *gym;
                        beststn = stn;
                        cout << "Maximum number of windows on day: " << stn.maxwindsday << endl;
                        cout << "Current total windows on week: " << stn.totalwinds << endl;
                    }
                    else if (stn.maxwinds == minmaxwinds && stn.maxwindsday == minmaxwindsday &&
                             stn.totalwinds < mintotalwinds)
                    {
                        mintotalwinds = stn.totalwinds;
                        bestclasses = classes;
                        bestteachers = teachers;
                        bestrooms = rooms;
                        *bestgym = *gym;
                        beststn = stn;
                        cout << "Current total windows on week: " << stn.totalwinds << endl;
                    }
                }
                step++;

                m.unlock();
            }
            else
            {
                m.lock();

                cout << "Step " << step + 1 << endl;

                if (res_ind < static_cast<int>(resclasses.size()))
                {
                    resclasses[res_ind] = classes;
                    resteachers[res_ind] = teachers;
                    resrooms[res_ind] = rooms;
                    resgym[res_ind] = *gym;
                    res_ind++;
                }

                step++;

                m.unlock();
            }
        }
        else
        {
            m.lock();
            bugstep++;
            m.unlock();
        }

        classes = classes2;
        teachers = teachers2;
        rooms = rooms2;
        *gym = *gym2;
    }
}

namespace
{
struct SessionData
{
    vector<Teacher> teachers;
    vector<Group> classes;
    vector<Classroom> rooms;
    Gym gym;

    SessionData() : teachers(100), classes(50), rooms(50), gym{} {}
};

class ConfigLoader
{
public:
    Settings create_defaults() const
    {
        Settings settings{};
        settings.begin_t = clock();
        settings.debug = false;
        settings.debug_fstpl = false;
        settings.checkday = false;
        settings.improve_timetable = false;
        settings.bugday = 0;
        settings.bugclass = 0;
        settings.countclasses = 0;
        settings.countteachers = -1;
        settings.threads = 0;
        settings.days = 0;
        settings.maxstep = 0;
        settings.randtype = 0;
        settings.maxwinds = 0;
        settings.maxwindsday = 0;
        settings.totalwinds = 0;
        settings.reset_days = false;
        settings.errors_limit = 0;
        settings.tofile = false;
        settings.random_seed = false;
        settings.last_day_short = false;
        settings.sessions = 1;
        settings.maxlessons = 0;
        settings.currsession = 1;
        settings.physicalcultname = "";
        settings.debug_fname = "";
        settings.fname = "";
        settings.methodicald_fname = "";
        settings.classrooms_fname = "";
        settings.output_fname = "";
        return settings;
    }

    void load_from_file(Settings &settings, const string &path) const
    {
        daynames.clear();
        doubles.clear();
        entirecourseinday.clear();
        notfirstlast.clear();
        conflicts.clear();

        ifstream conf(path);
        string line;
        while (getline(conf, line))
        {
            stringstream is_line(line);
            string key;
            if (!getline(is_line, key, '=')) continue;

            string val;
            if (!getline(is_line, val)) continue;

            const string trimmed_key = trim(key);
            if (trimmed_key == "days") settings.days = stoi(val);
            else if (trimmed_key == "steps") settings.maxstep = stoi(val);
            else if (trimmed_key == "debug") settings.debug = stoi(val);
            else if (trimmed_key == "debug_fstpl") settings.debug_fstpl = stoi(val);
            else if (trimmed_key == "checkday") settings.checkday = stoi(val);
            else if (trimmed_key == "bugday") settings.bugday = stoi(val);
            else if (trimmed_key == "bugclass") settings.bugclass = stoi(val);
            else if (trimmed_key == "reset_days") settings.reset_days = stoi(val);
            else if (trimmed_key == "errors_limit") settings.errors_limit = stoi(val);
            else if (trimmed_key == "randtype") settings.randtype = stoi(val);
            else if (trimmed_key == "random_seed") settings.random_seed = stoi(val);
            else if (trimmed_key == "last_day_short") settings.last_day_short = stoi(val);
            else if (trimmed_key == "improve_timetable") settings.improve_timetable = stoi(val);
            else if (trimmed_key == "threads") settings.threads = stoi(val);
            else if (trimmed_key == "maxlessons") settings.maxlessons = stoi(val);
            else if (trimmed_key == "sessions") settings.sessions = stoi(val);
            else if (trimmed_key == "double_lessons")
            {
                stringstream stream(val);
                string segment;
                while (getline(stream, segment, '/')) doubles.push_back(trim(segment));
            }
            else if (trimmed_key == "not_first_last")
            {
                stringstream stream(val);
                string segment;
                while (getline(stream, segment, '/')) notfirstlast.push_back(trim(segment));
            }
            else if (trimmed_key == "conflicts")
            {
                stringstream stream(val);
                string segment;
                while (getline(stream, segment, ','))
                {
                    stringstream stream1(segment);
                    string val1, val2;
                    getline(stream1, val1, '/');
                    getline(stream1, val2, '/');
                    val1 = trim(val1);
                    val2 = trim(val2);
                    conflicts[val1] = val2;
                    conflicts[val2] = val1;
                }
            }
            else if (trimmed_key == "entire_course_per_day")
            {
                stringstream stream(val);
                string segment;
                while (getline(stream, segment, '/')) entirecourseinday.push_back(trim(segment));
            }
            else if (trimmed_key == "file") settings.fname = trim(val);
            else if (trimmed_key == "physical_culture_name") settings.physicalcultname = trim(val);
            else if (trimmed_key == "days_of_the_week")
            {
                stringstream stream(val);
                string segment;
                while (getline(stream, segment, '/')) daynames.push_back(trim(segment));
            }
            else if (trimmed_key == "tofile") settings.tofile = stoi(val);
            else if (trimmed_key == "output_file") settings.output_fname = trim(val);
            else if (trimmed_key == "methodical_days_file") settings.methodicald_fname = trim(val);
            else if (trimmed_key == "classrooms_file") settings.classrooms_fname = trim(val);
            else if (trimmed_key == "debug_file") settings.debug_fname = trim(val);
        }

        settings.output_fname = get_available_fname(settings.output_fname);
        settings.debug_fname = get_available_fname(settings.debug_fname);
    }
};

class SessionBuilder
{
public:
    SessionData build(Settings &settings) const
    {
        SessionData data;

        ifstream file(settings.fname);
        vector<int> badclasses;

        CSVRow row;
        file >> row;
        for (size_t i = 2; i < row.size(); i++)
        {
            const int class_index = static_cast<int>(i);
            if (atoi(row[i].c_str()) != settings.currsession) badclasses.push_back(class_index);
        }

        file >> row;
        settings.countclasses = static_cast<int>(row.size() - 2 - badclasses.size());
        int t = 0;
        for (size_t i = 2; i < row.size(); i++)
        {
            const int class_index = static_cast<int>(i);
            if (find(badclasses.begin(), badclasses.end(), class_index) != badclasses.end()) continue;
            data.classes[t].name = row[i];
            data.classes[t].session = settings.currsession;
            const auto after = row[i].find_first_not_of(" 0123456789");
            if (after != string::npos)
            {
                data.classes[t].course = atoi(row[i].substr(0, after).c_str());
                data.classes[t].identifier = row[i].substr(after, row[i].length() - after);
            }
            t++;
        }

        file >> row;
        t = 0;
        for (size_t i = 2; i < row.size(); i++)
        {
            const int class_index = static_cast<int>(i);
            if (find(badclasses.begin(), badclasses.end(), class_index) != badclasses.end()) continue;
            if (row[i] == "1") data.classes[t].doublelessons = true;
            t++;
        }

        file >> row;
        string t_name;
        while (file >> row)
        {
            if (trim(row[0]) != "")
            {
                t_name = row[0];
                settings.countteachers++;
                data.teachers[settings.countteachers].name = t_name;
                for (int i = 0; i < settings.days; i++)
                {
                    for (int j = 0; j < global_maxlessons; j++)
                    {
                        data.teachers[settings.countteachers].availability[i][j] = true;
                        data.teachers[settings.countteachers].lesson[i][j] = &EmptyLessonStruct;
                    }
                }
            }

            t = 0;
            for (size_t i = 2; i < row.size(); i++)
            {
                const int class_index = static_cast<int>(i);
                if (find(badclasses.begin(), badclasses.end(), class_index) != badclasses.end()) continue;
                if (trim(row[i]) == "")
                {
                    t++;
                    continue;
                }

                string s_name = trim(row[1]);
                bool found_subgroup = false;
                int subgroup_index = 0;
                for (int p = 0; p < data.classes[t].countsubjects; p++)
                {
                    if (data.classes[t].subject[p].name == s_name &&
                        data.classes[t].subject[p].countlessons == atoi(row[i].c_str()))
                    {
                        data.classes[t].subject[p].issubgroups = true;
                        subgroup_index = p;
                        found_subgroup = true;
                        break;
                    }
                }

                if (found_subgroup)
                {
                    data.classes[t].subject[subgroup_index].teacherid2 = settings.countteachers;
                    t++;
                    continue;
                }

                string hours = row[i];
                if (hours.find('/') != string::npos)
                {
                    stringstream stream(hours);
                    string segment;
                    int pid = 1;
                    while (getline(stream, segment, '/'))
                    {
                        if (atoi(trim(segment).c_str()) != 0)
                        {
                            data.classes[t].subject[data.classes[t].countsubjects].name = s_name;
                            data.classes[t].subject[data.classes[t].countsubjects].teacherid = settings.countteachers;
                            data.classes[t].subject[data.classes[t].countsubjects].teacherid2 = 99;
                            data.classes[t].subject[data.classes[t].countsubjects].profilegroup = pid;
                            data.classes[t].subject[data.classes[t].countsubjects].countlessons = atoi(trim(segment).c_str());
                            if (pid != 2) data.classes[t].countlessonsweek += atoi(trim(segment).c_str());
                            data.classes[t].countsubjects++;
                        }
                        pid++;
                    }
                }
                else
                {
                    data.classes[t].subject[data.classes[t].countsubjects].name = s_name;
                    if (row[1] == settings.physicalcultname)
                    {
                        data.classes[t].subject[data.classes[t].countsubjects].isphysicalculture = true;
                    }
                    data.classes[t].subject[data.classes[t].countsubjects].teacherid = settings.countteachers;
                    data.classes[t].subject[data.classes[t].countsubjects].teacherid2 = 99;
                    data.classes[t].subject[data.classes[t].countsubjects].countlessons = atoi(hours.c_str());
                    data.classes[t].countlessonsweek += atoi(hours.c_str());
                    data.classes[t].countsubjects++;
                }
                t++;
            }
        }
        settings.countteachers++;

        for (unsigned int k = 0; k < data.rooms.size(); k++)
        {
            for (int i = 0; i < settings.days; i++)
            {
                for (int j = 0; j < global_maxlessons; j++)
                {
                    data.rooms[k].availability[i][j] = true;
                    data.rooms[k].lesson[i][j] = &EmptyLessonStruct;
                }
            }
        }

        for (int k = 0; k < settings.countclasses; k++)
        {
            for (int i = 0; i < settings.days; i++)
            {
                for (int j = 0; j < global_maxlessons; j++)
                {
                    data.classes[k].lesson[i][j] = &EmptyLessonStruct;
                }
            }
        }

        ifstream method_file(settings.methodicald_fname);
        method_file >> row;

        string tname, mdays, mday, segment;
        while (method_file >> row)
        {
            tname = trim(row[0]);
            mdays = trim(row[1]);
            if (mdays == "") continue;

            for (int i = 0; i < settings.countteachers; i++)
            {
                if (data.teachers[i].name != tname) continue;
                stringstream stream(mdays);
                while (getline(stream, segment, ';'))
                {
                    mday = trim(segment);
                    for (int j = 0; j < settings.days; j++)
                    {
                        if (daynames[j] == mday)
                        {
                            data.teachers[i].methodday[j] = true;
                            break;
                        }
                    }
                }
                break;
            }
        }

        ifstream classrooms_file(settings.classrooms_fname);
        classrooms_file >> row;

        string rnums;
        int rnum = 0;
        while (classrooms_file >> row)
        {
            tname = trim(row[0]);
            rnums = trim(row[1]);
            if (rnums == "" || rnums == "S" || rnums == "T") continue;

            for (int i = 0; i < settings.countteachers; i++)
            {
                if (data.teachers[i].name != tname) continue;
                stringstream stream(rnums);
                while (getline(stream, segment, ';'))
                {
                    rnum = atoi(trim(segment).c_str());
                    data.teachers[i].classroomslist[data.teachers[i].countrooms] = rnum;
                    data.teachers[i].countrooms++;
                }
                break;
            }
        }

        for (int k = 0; k < settings.countclasses; k++)
        {
            for (int i = 0; i < settings.days; i++)
            {
                for (int j = 0; j < settings.maxlessons; j++)
                {
                    data.classes[k].lesson[i][j] = &EmptyLessonStruct;
                }
            }
        }

        data.teachers[99].name = "Nonexistent teacher";
        for (int i = 0; i < settings.days; i++)
        {
            for (int j = 0; j < global_maxlessons; j++)
            {
                data.teachers[99].availability[i][j] = true;
                data.teachers[99].lesson[i][j] = &EmptyLessonStruct;
            }
        }

        for (int i = 0; i < settings.days; i++)
        {
            for (int j = 0; j < settings.maxlessons; j++)
            {
                data.gym.availability[i][j] = true;
            }
        }

        return data;
    }
};

class SessionExecutor
{
public:
    void run(SessionData &data, const Settings &settings) const
    {
        vector<thread> workers(settings.threads);
        for (int t = 0; t < settings.threads; t++)
        {
            workers[t] = thread(generate_timetable, data.classes, data.teachers, data.rooms, &data.gym, settings);
        }
        for (int t = 0; t < settings.threads; t++) workers[t].join();
    }
};

class SessionMerger
{
public:
    int merge(Settings stn[2]) const
    {
        if (stn[0].sessions <= 1) return 0;

        cout << "Start merging all sessions!" << endl;

        vector<Teacher> teachers(100);
        vector<Group> classes(50);
        vector<Classroom> rooms(50);
        Gym gym{};

        vector<Teacher> tempteachers(100);
        vector<Group> tempclasses(50);
        vector<Classroom> temprooms(50);
        Gym tempgym{};

        step = 0;
        int res_number = 0;
        const int cnt_second_ses = stn[1].countclasses;
        stn[0].countclasses += cnt_second_ses;

        const int available_results = min(res_ind, static_cast<int>(resclasses.size()));
        const int first_session_results = min(5, available_results);
        const int second_session_start = first_session_results;
        const int second_session_end = min(second_session_start + stn[0].maxstep, available_results);

        for (int i = 0; i < first_session_results; i++)
        {
            for (int j = second_session_start; j < second_session_end; j++)
            {
                teachers = resteachers[i];
                classes = resclasses[i];
                rooms = resrooms[i];
                gym = resgym[i];

                tempteachers = resteachers[j];
                tempclasses = resclasses[j];
                temprooms = resrooms[j];
                tempgym = resgym[j];

                for (int k = 0; k < cnt_second_ses; k++)
                {
                    for (int day = 0; day < stn[0].days; day++)
                    {
                        for (int les = stn[0].maxlessons - 1; les >= 0; les--)
                        {
                            tempclasses[k].timeslot[day][les + stn[0].maxlessons - 1] = tempclasses[k].timeslot[day][les];
                            tempclasses[k].lesson[day][les + stn[0].maxlessons - 1] = tempclasses[k].lesson[day][les];
                            tempclasses[k].timeslot[day][les] = false;
                            tempclasses[k].lesson[day][les] = &EmptyLessonStruct;
                        }
                    }
                }

                for (int k = 0; k < cnt_second_ses; k++)
                {
                    classes[stn[0].countclasses + k - cnt_second_ses] = tempclasses[k];
                }

                bool flag = true;
                for (int day = 0; day < stn[0].days && flag; day++)
                {
                    for (int les = 0; les < stn[0].maxlessons && flag; les++)
                    {
                        for (int k = 0; k < stn[0].countteachers; k++)
                        {
                            if (!teachers[k].availability[day][stn[0].maxlessons - 1] &&
                                !tempteachers[k].availability[day][0])
                            {
                                cout << "Wrong concatenation with teachers!" << endl;
                                flag = false;
                                break;
                            }
                        }

                        for (unsigned int k = 0; k < rooms.size(); k++)
                        {
                            if (!rooms[k].availability[day][stn[0].maxlessons - 1] &&
                                !temprooms[k].availability[day][0])
                            {
                                cout << "Wrong concatenation with rooms!" << endl;
                                flag = false;
                                break;
                            }
                        }

                        if (!gym.availability[day][stn[0].maxlessons - 1] && !tempgym.availability[day][0])
                        {
                            cout << "Wrong concatenation with gym!" << endl;
                            flag = false;
                            break;
                        }
                    }
                }
                if (!flag) continue;

                for (int day = 0; day < stn[0].days; day++)
                {
                    for (int les = 0; les < stn[0].maxlessons; les++)
                    {
                        for (int k = 0; k < stn[0].countteachers; k++)
                        {
                            teachers[k].availability[day][les + stn[0].maxlessons - 1] = tempteachers[k].availability[day][les];
                            teachers[k].lesson[day][les + stn[0].maxlessons - 1] = tempteachers[k].lesson[day][les];
                        }

                        for (unsigned int k = 0; k < rooms.size(); k++)
                        {
                            rooms[k].availability[day][les + stn[0].maxlessons - 1] = temprooms[k].availability[day][les];
                            rooms[k].lesson[day][les + stn[0].maxlessons - 1] = temprooms[k].lesson[day][les];
                        }

                        gym.availability[day][les + stn[0].maxlessons - 1] = tempgym.availability[day][les];
                        gym.lesson[day][les + stn[0].maxlessons - 1] = tempgym.lesson[day][les];
                    }
                }

                res_number++;
                Settings tempstn = stn[0];
                tempstn.maxlessons = stn[0].maxlessons * 2;
                optimize_timetable(classes, teachers, rooms, gym, tempstn);

                cout << "Step " << step + 1 << endl;
                if (tempstn.maxwinds <= minmaxwinds)
                {
                    if (tempstn.maxwinds < minmaxwinds)
                    {
                        minmaxwinds = tempstn.maxwinds;
                        minmaxwindsday = tempstn.maxwindsday;
                        mintotalwinds = tempstn.totalwinds;
                        bestclasses = classes;
                        bestteachers = teachers;
                        bestrooms = rooms;
                        *bestgym = gym;
                        beststn = tempstn;
                        cout << "Maximum number of windows on week: " << tempstn.maxwinds << endl;
                        cout << "Maximum number of windows on day: " << tempstn.maxwindsday << endl;
                        cout << "Current total windows on week: " << tempstn.totalwinds << endl;
                    }
                    else if (tempstn.maxwinds == minmaxwinds && tempstn.maxwindsday < minmaxwindsday)
                    {
                        mintotalwinds = tempstn.totalwinds;
                        minmaxwindsday = tempstn.maxwindsday;
                        bestclasses = classes;
                        bestteachers = teachers;
                        bestrooms = rooms;
                        *bestgym = gym;
                        beststn = tempstn;
                        cout << "Maximum number of windows on day: " << tempstn.maxwindsday << endl;
                        cout << "Current total windows on week: " << tempstn.totalwinds << endl;
                    }
                    else if (tempstn.maxwinds == minmaxwinds && tempstn.maxwindsday == minmaxwindsday &&
                             tempstn.totalwinds < mintotalwinds)
                    {
                        mintotalwinds = tempstn.totalwinds;
                        bestclasses = classes;
                        bestteachers = teachers;
                        bestrooms = rooms;
                        *bestgym = gym;
                        beststn = tempstn;
                        cout << "Current total windows on week: " << tempstn.totalwinds << endl;
                    }
                }
                step++;
            }
        }

        return res_number;
    }
};
}

int SchedMeshApp::run()
{
    reset_runtime_state();

    ConfigLoader config_loader;
    SessionBuilder session_builder;
    SessionExecutor session_executor;
    SessionMerger session_merger;

    Settings stn[2];
    stn[0] = config_loader.create_defaults();
    config_loader.load_from_file(stn[0], "data/settings.conf");
    stn[1] = stn[0];

    for (int s = 1; s <= stn[s - 1].sessions; s++)
    {
        step = 0;
        stn[s - 1].currsession = s;
        SessionData session_data = session_builder.build(stn[s - 1]);
        session_executor.run(session_data, stn[s - 1]);
    }

    int res_number = session_merger.merge(stn);

    if (stn[0].sessions == 1 || (stn[0].sessions > 1 && res_number >= 1))
    {
        print_full_info(bestclasses, bestteachers, bestrooms, *bestgym, beststn, step, bugstep, brokendays, 1);
    }
    else
    {
        cout << "No results" << endl;
    }

    return 0;
}
int main()
{
    SchedMeshApp app;
    return app.run();
}
