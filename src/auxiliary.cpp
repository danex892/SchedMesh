#include "schedmesh.h"

string remove_surrounding_quotes(string &s)
{
    string res = s;
    if (*s.begin() == '"')
    {
        if (*s.rbegin() == '"')
        {
            res = s.substr(1, s.length() - 2);
        }
        else
        {
            res = s.substr(1, s.length() - 1);
        }
    }
    else
    {
        if (*s.begin() == '"')
        {
            res = s.substr(0, s.length() - 1);
        }
    }
    return res;
}

string trim(string s)
{
    const size_t first = s.find_first_not_of(' ');
    if (first == string::npos) return "";
    const size_t last = s.find_last_not_of(' ');
    return s.substr(first, last - first + 1);
}

bool is_double(string &x)
{
    bool res = false;
    if (find(doubles.begin(), doubles.end(), x) != doubles.end()) {
        res = true;
    }
    return res;
}

bool file_exists(const string &name)
{
    ifstream file(name);
    return file.good();
}

string get_available_fname(string &fname)
{
    string o_name = fname.substr(0, fname.find_last_of(".")); // timetable file name
    string o_ext = fname.substr(fname.find_last_of(".")); // timetable file extension
    string res = fname;

    if (file_exists(o_name + o_ext))
    {
        int fi = 1;
        while (file_exists(o_name + "_" + to_string(fi) + o_ext))
        {
            fi++;
        }
        res = o_name + "_" + to_string(fi) + o_ext;
    }
    return res;
}

int calc_winds(Teacher &t, int d, int lessons)
{
    int winds = 0;
    int l = 0;
    int r = lessons - 1;
    while (t.availability[d][l]) l++;
    while (t.availability[d][r]) r--;
    for (int j = l; j <= r; j++)
    {
        if (t.availability[d][j]) winds++;
    }
    return winds;
}

void get_subject_num(Group &g, int tid, int tid1, string &name, string &name1, int &n1, int &n2)
{
    n1 = n2 = -1;
    bool r1, r2;
    r1 = r2 = false;
    for (int p = 0; p < g.countsubjects; p++) // subject number
    {
        if (g.subject[p].name == name && g.subject[p].teacherid == tid)
        {
            r1 = true;
            n1 = p;
        }
        if (g.subject[p].name == name1 && g.subject[p].teacherid == tid1)
        {
            r2 = true;
            n2 = p;
        }
        if ((r1 && r2) || (r1 && name1 == "")) break;
    }
}

int get_classroom_of_teacher(Teacher &t, int tid, int d, int l)
{
    if (t.lesson[d][l]->teacherid == tid)
    {
        return t.lesson[d][l]->classroom;
    }
    else
    {
        return t.lesson[d][l]->classroom2;
    }
}

bool check_first_last(Group &g, int t, int l, Settings &stn)
{
    bool res = false;

    string lname = g.subject[t].name;
    if (find(notfirstlast.begin(), notfirstlast.end(), lname) != notfirstlast.end()) {
        const int last_lesson = stn.maxlessons - 1;
        if (l != 0 && l != last_lesson)
        {
            res = true;
        }
    }
    else
    {
        res = true;
    }

    return res;
}

bool check_conflicts(Group &g, int t, int d, int l)
{
    bool res = true;
    string l1name = g.subject[t].name;
    map<string,string>::const_iterator it = conflicts.find(l1name);
    if (it != conflicts.end())
    {
        string l2name = conflicts[l1name];
        for (int j = 0; j < l; j++)
        {
            if (g.lesson[d][j]->name == l2name)
            {
                res = false;
                break;
            }
        }
    }
    return res;
}

bool check_physical_culture(Group &g, Gym &gym, int t, int d, int l)
{
    bool res = true;
    if (g.subject[t].isphysicalculture)
    {
        if (gym.availability[d][l])
        {
            if (gym.lesson[d][l].group1course != 0)
            {
                if (gym.lesson[d][l].group1course != g.course
                    && gym.lesson[d][l].group1course != g.course + 1
                    && gym.lesson[d][l].group1course != g.course - 1)
                {
                    res = false;
                }
            }
        }
        else
        {
            res = false;
        }
    }
    return res;
}

bool check_double_lesson(Group &g, Teacher &t, int s, int d, int l)
{
    bool res = true;
    if (is_double(g.subject[s].name))
    {
        if (!g.timeslot[d][l + 1] ||
            !t.availability[d][l + 1])
        {
            res = false;
        }
    }
    return res;
}

bool check_profile_lesson(Group &g, vector<Teacher> &t, int t1, int &t1s2, int s, int &s1, int d, int l, int randtype)
{
    bool res = false;
    if (g.subject[s].profilegroup != 0)
    {
        int pnum = 0;
        if (g.subject[s].profilegroup == 1)
        {
            pnum = 2;
            int A[60];
            int cnt = 0;
            for (int p = 0; p < g.countsubjects; p++)
            {
                if (g.subject[p].profilegroup == pnum && g.subject[p].countlessons > 0)
                {
                    A[cnt] = p;
                    cnt++;
                }
            }

            if (randtype == 0)
            {
                shuffle(&A[0], &A[cnt], rng_stand);
            }
            else
            {
                shuffle(&A[0], &A[cnt], rng);
            }

            for (int p = 0; p < cnt; p++)
            {
                int tid = g.subject[A[p]].teacherid;
                if (t1 != tid && t[tid].availability[d][l]
                        && !t[tid].methodday[d]
                        && (g.subject[A[p]].day[d] == 0 || g.doublelessons))
                {
                    s1 = A[p];
                    t1s2 = tid;
                    res = true;
                    break;
                }
            }
        }
        else
        {
            cout << "Something wrong! We try to check profile lesson with profile = 2!" << endl;
        }
    }
    else
    {
        res = true;
    }
    return res;
}

void move_teacher_from_to_room(vector<Classroom> &r, int d, int l, int t1, int r1, int r2)
{
    if (t1 != 0)
    {
        Lesson* new_les = r[r1].lesson[d][l];

        if (t1 == new_les->teacherid)
        {
            new_les->classroom = r2;
        }
        else
        {
            new_les->classroom2 = r2;
        }

        r[r1].availability[d][l] = true;
        r[r1].lesson[d][l] = &EmptyLessonStruct;
        r[r2].availability[d][l] = false;
        r[r2].lesson[d][l] = new_les;
    }
}

// t1 - number of teacher which needs a room
// d, l - day and lesson numbers
// res_r - founded room
// res_r1, res_t2 - room and teacher to swap
// not_r - room for paired lesson
bool check_room(vector<Classroom> &rooms, vector<Teacher> &t, int t1, int d, int l, int &res_r, int &res_r1,
                int &res_t2, int arr_r[], int cnt)
{
    bool res = false;
    if (t[t1].countrooms != 0)
    {
        for (int i = 0; i < t[t1].countrooms; i++)
        {
            if (res) break;
            int r = t[t1].classroomslist[i];
            bool conflict = false;
            for (int j = 0; j < cnt; j++)
            {
                if (arr_r[j] == r)
                {
                    conflict = true;
                    break;
                }
            }
            if (conflict) continue;
            if (rooms[r].availability[d][l])
            {
                res_r = r;
                res = true;
                break;
            }
            else
            {
                int t2 = 0;
                if (rooms[r].lesson[d][l]->classroom == r)
                {
                    t2 = rooms[r].lesson[d][l]->teacherid;
                }
                else
                {
                    t2 = rooms[r].lesson[d][l]->teacherid2;
                }

                for (int j = 0; j < t[t2].countrooms; j++)
                {
                    int r1 = t[t2].classroomslist[j];
                    conflict = false;
                    for (int k = 0; k < cnt; k++)
                    {
                        if (arr_r[k] == r1)
                        {
                            conflict = true;
                            break;
                        }
                    }
                    if (conflict) continue;
                    if (rooms[r1].availability[d][l])
                    {
                        res_r = r;
                        res_r1 = r1;
                        res_t2 = t2;
                        res = true;
                        break;
                    }
                }
            }
        }
    }
    else
    {
        res = true;
    }
    return res;
}

bool improve_timetable(vector<Group> &g, vector<Teacher> &t, vector<Classroom> &rooms, Gym &gym, Settings &stn)
{

    bool res = false;
    for (int j = 0; j < stn.countclasses; j++)
    {
        for (int d = 0; d < stn.days; d++)
        {
            for (int i = 0; i < stn.maxlessons; i++)
            {
                if (g[j].timeslot[d][i])
                {
                    for (int d1 = d; d1 < stn.days; d1++)
                    {
                        for (int i1 = i + 1; i1 < stn.maxlessons; i1++)
                        {
                            if (g[j].timeslot[d1][i1])
                            {
                                if (swap_lessons(g, t, rooms, gym, stn, j, d, i, d1, i1, 1))
                                {
                                    res = true;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return res;
}

void optimize_timetable(vector<Group> &g, vector<Teacher> &t, vector<Classroom> &rooms, Gym &gym, Settings &stn)
{
    int p = 1;
    for (vector<Teacher>::iterator it = t.begin(); it != t.end(); ++it)
    {
        for (int i = 0; i < stn.days; i++)
        {
            int l = 0;
            int r = stn.maxlessons - 1;
            while (it->availability[i][l]) l++;
            while (it->availability[i][r]) r--;
            for (int j = l; j <= r; j++)
            {
                if (it->availability[i][j]) it->winds[i]++;
                else it->hours++;
            }
            it->totalwinds += it->winds[i];
        }
        if (it->totalwinds > stn.maxwinds)
        {
            stn.maxwinds = it->totalwinds;
        }
        if (p == stn.countteachers) break;
        p++;
    }
    if (stn.improve_timetable)
    {
        bool res = false;
        while (true)
        {
            res = improve_timetable(g, t, rooms, gym, stn);
            if (!res) break;
        }

        p = 1;
        stn.maxwinds = 0;
        for (vector<Teacher>::iterator it = t.begin(); it != t.end(); ++it)
        {
            for (int i = 0; i < stn.days; i++)
            {
                if (it->winds[i] > stn.maxwindsday)
                {
                    stn.maxwindsday = it->winds[i];
                }
            }
            if (it->totalwinds > stn.maxwinds)
            {
                stn.maxwinds = it->totalwinds;
            }
            stn.totalwinds += it->totalwinds;
            if (p == stn.countteachers) break;
            p++;
        }
    }
}
