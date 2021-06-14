#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <getopt.h>
#include <iomanip>
#include <iostream>
#include <algorithm>
#include <regex>
#include <signal.h>

#define MAX_CLIENTS 128

using namespace std;

class Server
{
public:
    Server(int port)
    {
        tcp_sock = get_listen_sock(port, "tcp");
        udp_sock = get_listen_sock(port, "udp");
    }
    int listening()
    {
        signal(SIGINT, Server::interrupt);    // обработка сигнала Ctrl+v
        struct pollfd fd[MAX_CLIENTS + 2];
        int numclients = 0; 
        fd[0].fd = tcp_sock;
        fd[0].events = POLLIN;
        fd[1].fd = udp_sock;
        fd[1].events = POLLIN;
        for (int i = 2; i < MAX_CLIENTS + 2; i++)
        {
            fd[i].fd = -1;
        }
        while(!exit_from_user)
        {
            int ret = poll( fd, numclients + 2, -1 );
            if ( ret < 0 )
            {
                if (!exit_from_user) cout << "Error: poll" << endl;
                break;
            }
            else if ( ret == 0 )
            {
                cout << "Time out" << endl;
                break;
            }
            else
            {
                if ( fd[0].revents & POLLIN )     // новое подключение по tcp
                    add_tcp_socket(fd, numclients);
                if ( fd[1].revents & POLLIN )     // новое подключение по udp
                {
                    struct sockaddr_in addr;
                    char* data = read_data(fd[1].fd, "udp", addr);
                    string tmp = prepare_data(data);
                    send_data(0, tmp, "udp",(sockaddr&) addr);
                    delete[] data;
                }
                for (int i = 2; i <= numclients; i++)
                {
                    if (fd[i].fd < 0)
                        continue;
                    if(fd[i].revents & POLLHUP)    // получен сигнал об отключении клиента
                    {
                        fd[i].revents = 0;
                        close(fd[i].fd);
                        fd[i].fd = -1;
                    }
                    else if(fd[i].revents & POLLIN)
                    {
                        struct sockaddr_in addr;
                        char *data = read_data(fd[i].fd, "tcp", addr);
                        if (data == nullptr)
                        {
                            fd[i].revents = 0;
                            close(fd[i].fd);
                            fd[i].fd = -1;
                            continue;
                        }
                        string tmp = prepare_data(data);
                        send_data(fd[i].fd, tmp, "tcp",(sockaddr&) addr);
                        delete[] data;
                    }
                }

            }
        }
        return 1;
    }
    static void interrupt(int sig)
    {
        exit_from_user = 1;
    }
    static bool comp(string first, string second)
    {
        return stoi(first) < stoi(second);
    }
    ~Server()
    {
        close(tcp_sock);
        close(udp_sock);
    }
    

private:
    int get_listen_sock(int port, string mode)
    {
        int listener;
        struct sockaddr_in addr;

        listener = socket(AF_INET, (mode == "tcp") ? SOCK_STREAM : SOCK_DGRAM, 0);
        if(listener < 0)
        {
            perror("socket");
            exit(1);
        }       
        int enable = 1;

        int fl = fcntl(listener, F_GETFL, 0);
        fcntl(listener, F_SETFL, fl | O_NONBLOCK);
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;
        if(bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0 || (mode == "tcp") ? listen(listener, 1) < 0 : 0 )
        {
            perror("bind");
            exit(2);
        }

        return listener;
    }
    int add_tcp_socket(struct pollfd* fd, int& numclients)
    {
        fd[0].revents = 0;
        int sock = accept(fd[0].fd, NULL, NULL);
        if(sock < 0)
        {
            perror("accept");
            exit(3);
        }
        int i;
        for(i = 1; i < MAX_CLIENTS + 2; i++)
        {
            if(fd[i].fd < 0)
            {
                fd[i].fd = sock;
                fd[i].events = POLLIN | POLLHUP;
                break;
            }
        }
        if(i == MAX_CLIENTS + 2)
            return 0;
        if (i > numclients)
            numclients = i;
        return 1;
    }
    int send_data(int socket, string data, string protocol, struct sockaddr& addr)
    {
        size_t len = data.size() + 1;
        int ret;
        if (protocol == "tcp")
        {
            if (send(socket, &len, sizeof(size_t), 0) == -1)
            {
                perror("send");
                exit(1);
            }
            if ((ret = send(socket, data.c_str(), len, 0)) < 0)
            {
                perror("send");
                exit(1);
            }
        }
        else if (protocol == "udp")
        {
            if (sendto(udp_sock, &len, sizeof(size_t), 0, (struct sockaddr*) &addr, sizeof(struct sockaddr_in)) == -1)
            {
                perror("sendto");
                exit(1);
            }
            if ((ret = sendto(udp_sock, data.c_str(), len, 0, (struct sockaddr*) &addr, sizeof(struct sockaddr_in))) < 0)
            {
                perror("sendto");
                exit(1);
            }
        }
        return 1;
    }
    char* read_data(int socket, string mode, struct sockaddr_in& addr)
    {
        size_t datalen;
        if (mode == "tcp")
        {
            int ret = recv(socket, &datalen, sizeof(size_t), 0);
            if (ret > 0)
            {
                char *data = new char[datalen];
                ret = recv(socket, data, datalen, 0);
                return data;
            }
        }
        else if (mode == "udp")
        {
            socklen_t addrlen = sizeof(addr);
            int received = recvfrom(socket, &datalen, sizeof(size_t), 0, (struct sockaddr*) &addr, &addrlen);
            if (received > 0)
            {
                char *data = new char[datalen];
                received = recvfrom(socket, data, datalen, 0, (struct sockaddr*) &addr, &addrlen);
                return data;
            }
        }
        return nullptr;
    } 
    bool isNumber(string s)
    {
        for (int a = 0; a < s.length(); a++)
        {            
            if ((s[a] < 48) || (s[a] > 57))  return false;   
        }
        return true;
    }
    string prepare_data(string data)  // извлечение чисел из строки и подсчет их суммы
    {
        data = regex_replace(data, regex("[^0-9a-z_]"), " ");  // исключаем незначащие знаки
        vector<std::string> words;
        istringstream ist(data);
        string tmp;
        int sum = 0;
        while ( ist >> tmp )
        {
            if (isNumber(tmp))
                words.push_back(tmp);
        }
        sort(words.begin(), words.end(), Server::comp);
        tmp = "";
        for (auto i: words)
        {
            tmp += i + " ";
            sum += stoi(i);
        }
        tmp += "\n" + to_string(sum);
        return tmp;
    }
    int tcp_sock, udp_sock;
    static volatile sig_atomic_t exit_from_user;
};
void usage(char* name)
{
    cout << "usage: " << name << endl;    
    cout << setw(60) << left << "  -h, --help " << "to this message" << endl;     
    cout << setw(60) << left << "  -p PORT_SERVER, p=PORT_SERVER" << "to set port of server" << endl;     
    cout << "example: " << name << " --port=4040" << endl;    
    return;
}

int get_parameters(int argc, char* argv[], int &port)
{
    int c;
    while (1)
    {
        static struct option long_opt[] = {
                            {"help", 0, 0, 'h'},
                            {"port", 1, 0, 'p'},
                            {0,0,0,0}};
        int optIdx;
        
        if((c = getopt_long(argc, argv, "hp:", long_opt, &optIdx)) == -1)
        break;
        switch( c )
        {        
            case 'p':
                port = atoi(optarg);
                continue;
            
            case 'h':
                usage(argv[0]);
                continue;
            default:
                usage(argv[0]);
                return 1;
        }
    }
    return 0;
}

volatile sig_atomic_t Server::exit_from_user = 0;

int main(int argc, char *argv[])
{
    int port = -1;
    if (argc == 1)
    {
        usage(argv[0]);
        return 0;
    }
    if(get_parameters(argc, argv, port))
        return -1;
     if (port < 0 || port > 65535 )
    {
        cout << "Invalid port of server" << endl;
        return -1;
    }
    Server server(port);
    server.listening();
}