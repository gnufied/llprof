#ifndef LLPROF_HTTP_H
#define LLPROF_HTTP_H

#include <pthread.h>
#include <string>
#include <sstream>
#include <map>

bool recv_string(int sock, std::string &result, int maxlen = 1024);

void start_http_server();


// Json writer
inline void write_json(std::stringstream &strm, const std::string &value)
{
    strm << "\"";
    for(std::string::const_iterator iter = value.begin(); iter != value.end(); iter++)
    {
        switch(*iter)
        {
        case '\t':
            strm << "\\t";
            break;
        case '\r':
            strm << "\\r";
            break;
        case '\n':
            strm << "\\n";
            break;
        case '"':
            strm << "\\\"";
            break;
        default:
            strm << *iter;
            break;
        }
    }
    strm << "\"";
}

inline void write_json(std::stringstream &strm, const char *value)
{
    strm << "\"" << value << "\"";
}

inline void write_json(std::stringstream &strm, int value)
{
    strm << value;
}

inline void write_json(std::stringstream &strm, float value)
{
    strm << value;
}

inline void write_json(std::stringstream &strm, double value)
{
    strm << value;
}

inline void write_json(std::stringstream &strm, std::stringstream &json)
{
    strm << json.str();
}

/*
inline void write_json(std::stringstream &strm, bool flag)
{
    if(flag)
        strm << "true";
    else
        strm << "false";
}
*/


class JsonDict
{
    std::stringstream &strm_;
    bool closed_, first_;
public:
    JsonDict(std::stringstream &strm)
        :strm_(strm)
    {
        closed_ = false;
        first_ = true;
    }
    ~JsonDict()
    {
        close();
    }

    template<typename T>
    void add(std::string name, T val)
    {
        if(first_)
            strm_ << "{", first_ = false;
        else
            strm_ << ", ";
        write_json(strm_, name);
        strm_ << ": ";
        write_json(strm_, val);
    }
    
    void close()
    {
        if(!closed_)
        {
            if(first_)
                strm_ << "{";
            strm_ << "}";
            closed_ = true;
        }
    }
};


class JsonList
{
    std::stringstream &strm_;
    bool closed_, first_;
public:
    JsonList(std::stringstream &strm)
        :strm_(strm)
    {
        closed_ = false;
        first_ = true;
    }
    ~JsonList()
    {
        close();
    }
    template<typename T>
    void add(T val)
    {
        if(first_)
            strm_ << "[", first_ = false;
        else
            strm_ << ", ";
        write_json(strm_, val);
    }
    
    void close()
    {
        if(!closed_)
        {
            if(first_)
                strm_ << "[";
            strm_ << "]";
            closed_ = true;
        }
    }
    
    std::string str()
    {
        return strm_.str();
    }
};




template<typename TMap>
typename TMap::mapped_type getm(const TMap &m, typename TMap::key_type key, typename TMap::mapped_type default_value)
{
    typename TMap::const_iterator iter = m.find(key);
    if(iter == m.end())
        return default_value;
    return (*iter).second;
}



namespace llprof{
        
    template<typename TKey, typename TValue>
    std::ostream& operator<<(std::ostream& s, const std::map<TKey, TValue>& x)
    {
        for(typename std::map<TKey, TValue>::const_iterator iter = x.begin(); iter != x.end(); iter++)
        {
            s << (*iter).first << " = " << (*iter).second << std::endl;
        }
        return s;
    }


}

#endif //LLPROF_HTTP_H
