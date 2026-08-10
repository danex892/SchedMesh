#include "csvrow.h"

string const& CSVRow::operator[](size_t index) const
{
    if (index < m_data.size())
    {
        return m_data[index];
    }
    else
    {
        return default_val;
    }
}

size_t CSVRow::size() const
{
    return m_data.size();
}

void CSVRow::readNextRow(istream& str)
{
    string line;
    getline(str, line);

    stringstream lineStream(line);
    string cell;

    m_data.clear();
    while (getline(lineStream, cell, ','))
    {
        m_data.push_back(cell);
    }
}

CSVRow::CSVRow(void)
{
    default_val = "";
}

istream& operator>>(istream& str, CSVRow& data)
{
    data.readNextRow(str);
    return str;
}
