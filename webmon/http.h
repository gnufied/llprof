#ifndef LLPROF_HTTP_H
#define LLPROF_HTTP_H

#include <pthread.h>
#include <string>
#include <sstream>
#include <map>
#include <vector>
#include <assert.h>
bool recv_string(int sock, std::string &result, int maxlen = 1024);

void start_http_server();




// Json writer
inline void write_json(std::stringstream &strm, int value)
{
    strm << value;
}

inline void write_json(std::stringstream &strm, unsigned int value)
{
    strm << value;
}
inline void write_json(std::stringstream &strm, unsigned long long int value)
{
    strm << value;
}

inline void write_json(std::stringstream &strm, signed long long int value)
{
    strm << value;
}

inline void write_json(std::stringstream &strm, double value)
{
    strm << value;
}

inline void write_json(std::stringstream &strm, const char *value)
{
    strm << "\"" << value << "\"";
}

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


inline void write_json(std::stringstream &strm, std::stringstream &json)
{
    strm << json.str();
}


class JsonWriter
{
    std::stringstream *strm_;
    bool closed_, must_delete_;
protected:
    bool first_;
    std::stringstream &writer()
    {
        assert(!closed_);
        return *strm_;
    }

public:
    JsonWriter()
        :strm_(new std::stringstream())
    {
        closed_ = false;
        first_ = true;
        must_delete_ = true;
    }
    
    JsonWriter(std::stringstream &strm)
        :strm_(&strm)
    {
        closed_ = false;
        first_ = true;
        must_delete_ = false;
    }
    virtual ~JsonWriter()
    {
        close();
        if(must_delete_)
            delete strm_;
    }
    void close()
    {
        if(!closed_)
        {
            write_close();
            closed_ = true;
        }
    }
    
    virtual void write_close() = 0;
    std::stringstream &strm()
    {
        close();
        return *strm_;
    }

    std::string str()
    {
        return strm().str();
    }
};


class JsonDict: public JsonWriter
{
public:
    JsonDict() :JsonWriter(){}
    JsonDict(std::stringstream &strm) :JsonWriter(strm){}

    ~JsonDict(){close();}

    template<typename T>
    void add(std::string name, T val)
    {
        if(first_)
            writer() << "{", first_ = false;
        else
            writer() << ", ";
        write_json(writer(), name);
        writer() << ": ";
        write_json(writer(), val);
    }
    
    void write_close()
    {
        if(first_)
            writer() << "{";
        writer() << "}";
    }
    
};


class JsonList: public JsonWriter
{
public:
    JsonList() :JsonWriter(){}
    JsonList(std::stringstream &strm) :JsonWriter(strm){}
    ~JsonList(){close();}

    template<typename T>
    void add(T val)
    {
        if(first_)
            writer() << "[", first_ = false;
        else
            writer() << ", ";
        write_json(writer(), val);
    }

    void write_close()
    {
        if(first_)
            writer() << "[";
        writer() << "]";
    }

};

/*
inline void write_json(std::stringstream &strm, bool flag)
{
    if(flag)
        strm << "true";
    else
        strm << "false";
}
*/






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
