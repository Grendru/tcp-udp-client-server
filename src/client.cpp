#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <sys/poll.h>
#include <string.h>
#include <getopt.h>
#include <iomanip>


using namespace std;

class Client
{
public:
    Client(int ip, int port, string mode): protocol(mode)
    {
        sock = get_sock(ip, port, mode);
    }
    int send_data(string data)
    {
        if (protocol == "tcp")
        {
            size_t len = data.size();
            int ret;
            if (send(sock, &len, sizeof(size_t), 0) == -1)
            {
                perror("send");
                exit(1);
            }
            if ((ret = send(sock, data.c_str(), len, 0)) < 0)
            {
                cout << "ret: " << ret;
                perror("send");
                exit(1);
            }
        }
        else if (protocol == "udp")
        {
            size_t len = data.size();
            int ret;
            if (sendto(sock, &len, sizeof(size_t), 0, (struct sockaddr*) &addr, sizeof(struct sockaddr_in)) == -1)
            {
                perror("sendto");
                exit(1);
            }
            if ((ret = sendto(sock, data.c_str(), len, 0, (struct sockaddr*) &addr, sizeof(struct sockaddr_in))) < 0)
            {
                cout << "ret: " << ret;
                perror("sendto");
                exit(1);
            }
        }
        return 0;
    }    
    char* read_data()
    {
        size_t datalen;
        if (protocol == "tcp")
        {
            int ret = recv(sock, &datalen, sizeof(size_t), 0);
            if (ret > 0)
            {
                char *data = new char[datalen];
                ret = recv(sock, data, datalen, 0);
                return data;
            }
        }
        else if (protocol == "udp")
        {
            socklen_t addrlen = sizeof(addr);
            int received = recvfrom(sock, &datalen, sizeof(size_t), 0, (struct sockaddr*) &addr, &addrlen);
            if (received > 0)
            {
                char *data = new char[datalen];
                received = recvfrom(sock, data, datalen, 0, (struct sockaddr*) &addr, &addrlen);
                return data;
            }
        }
        return nullptr;
    } 
    ~Client()
    {
        close(sock);
    }
private:
    int get_sock(int ip, int port, string mode)
    {
        int new_sock;
        new_sock = socket(AF_INET, (mode == "tcp") ? SOCK_STREAM : SOCK_DGRAM, 0);
        if(new_sock < 0)
        {
            perror("socket");
            exit(1);
        }

        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = ip;
        if (mode == "tcp")
        {
            if(connect(new_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
            {
                perror("connect");
                exit(2);
            }
        }
        return new_sock;
    }
    int sock;
    string protocol;
    struct sockaddr_in addr;
};
void usage(char *name)
{
    cout << "usage: " << name << endl;    
    cout << setw(60) << left << "  -h, --help " << "to this message" << endl;     
    cout << setw(60) << left << "  -i IP_ADDRESS_SERVER, --ip=IP_ADDRESS_SERVER" << "to set ip address of server" << endl;
    cout << setw(60) << left << "  -p PORT_SERVER, p=PORT_SERVER" << "to set port of server" << endl;     
    cout << setw(60) << left << "  -u, --udp or -t, --tcp" << "to select protocol" << endl; 
    cout << "example: " << name << " --ip=127.0.0.1 --port=4040 -t" << endl;    
    return;
}
int get_parameters(int argc, char* argv[], string &ip,int &port, string &mode)
{
    int c;
    while (1)
    {
        static struct option long_opt[] = {
                            {"help", 0, 0, 'h'},
                            {"tcp", 0, 0, 't'},
                            {"udp", 0, 0, 'u'},
                            {"ip", 1, 0, 'i'},
                            {"port", 1, 0, 'p'},
                            {0,0,0,0}};
        int optIdx;
        
        if((c = getopt_long(argc, argv, "htui:p:", long_opt, &optIdx)) == -1)
        break;
        switch( c )
        {        
            case 't':
                if (mode == "udp")
                {
                    cout << "Please select one of the protocols: tcp or udp" << endl;
                    return 1;
                }
                mode = "tcp";
                continue;
            
            case 'u':
                if (mode == "tcp")
                {
                    cout << "Please select one of the protocols: tcp or udp" << endl;
                    return 1;
                }
                mode = "udp";
                continue;
                
            case 'i':
                ip = optarg;
                continue;
                
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
int main(int argc, char* argv[])
{
    string ip_str, mode;
    int ip = -1, port = -1;
    if (argc == 1)
    {
        usage(argv[0]);
        return 0;
    }
    if(get_parameters(argc, argv, ip_str, port, mode))
        return -1;
    if (port < 0 || port > 65535 )
    {
        cout << "Invalid port of server" << endl;
        return -1;
    }
    else if (inet_pton(AF_INET, ip_str.c_str(), &ip) != 1)
    {
        cout << "Invalid ip address of server" << endl;
        return -1;
    }
    else if (mode != "tcp" && mode != "udp")
    {
        cout << "Invalid protocol" << endl;
        return -1;
    }
    Client client(ip, port, mode);
    while(1) 
    {

        string data;
        getline(cin, data);
        client.send_data(data);
        char *datafromserver = client.read_data();
        cout << datafromserver << endl;
        delete[] datafromserver;
    }
    return 0;
}