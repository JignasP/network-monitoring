#include <stdio.h>      
#include <stdlib.h>     
#include <string.h>     
#include <time.h>       
#include <conio.h>      // For getting keyboard input
#include <windows.h>    
#include <winsock2.h>   
#include <ws2tcpip.h>   
#include <iphlpapi.h>   // For IP information
#include <tlhelp32.h>   // For process info

// for compiler to use these files
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")


#define LOG_FILE "network_monitoring_log.txt"  
#define UPDATE_INTERVAL 5                   
#define MAX_CONNECTIONS 100                 
#define MAX_NAME_LENGTH 256                 

//connection filters
typedef enum {
    FILTER_ALL,         // Show all connections
    FILTER_LOCAL,       // Show only local connections 
    FILTER_INTERNET,    // Show only remote connections 
    FILTER_ACTIVE,      // Show only connected connections
    FILTER_LISTENING,   // Show only ports waiting for connections
    FILTER_TCP,         // Show only TCP connections
    FILTER_UDP          // Show only UDP connections
} FilterType;

// info about a network connection
typedef struct {
    char localAddress[46];       // The IP address on your computer
    int localPort;               // The port on your computer
    char remoteAddress[46];      // The IP address of the other computer
    int remotePort;              // The port on the other computer
    char protocol[10];           // TCP or UDP
    char state[20];              // Connection state like LISTENING or ESTABLISHED
    char programName[MAX_NAME_LENGTH]; 
    DWORD programId;             // The ID number of the program
} NetworkConnection;


void getNetworkConnections(NetworkConnection connectionList[], int *connectionCount);
void getTcpConnections(NetworkConnection connectionList[], int *connectionCount);
void getUdpConnections(NetworkConnection connectionList[], int *connectionCount);
void showConnections(NetworkConnection connectionList[], int connectionCount, FilterType filter);
void saveConnectionsToLog(NetworkConnection connectionList[], int connectionCount);
void showMenu(FilterType currentFilter);
void showLogFile();
char* getConnectionStateName(DWORD state);
void getProgramName(DWORD processId, char* nameBuffer, int maxLength);
void clearConsoleScreen();
void showHeader();
void showHelp();
void showStatistics(NetworkConnection connectionList[], int connectionCount);
const char* getFilterName(FilterType filter);


int main() {
  
    NetworkConnection connectionList[MAX_CONNECTIONS];
    int connectionCount = 0;
    

    int refreshInterval = UPDATE_INTERVAL; 
    time_t lastUpdateTime = 0;  
    FilterType currentFilter = FILTER_ALL;
    int showHelpScreen = 0;
    int showStats = 0;
    
    // Initialize Windows Sockets
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("Error: Could not initialize network functions.\n");
        printf("Press any key to exit...");
        _getch(); 
        return 1;
    }
    

    clearConsoleScreen();
    showHeader();
    
    
    while (1) {

        if (difftime(time(NULL), lastUpdateTime) >= refreshInterval) {
           
            clearConsoleScreen();
            showHeader();
            
            // Get all the network connections and show them
            getNetworkConnections(connectionList, &connectionCount);
            showConnections(connectionList, connectionCount, currentFilter);

            saveConnectionsToLog(connectionList, connectionCount);
            

            if (showStats) showStatistics(connectionList, connectionCount);
            if (showHelpScreen) showHelp();
            

            lastUpdateTime = time(NULL);
        }
        
        // Show the menu at the bottom of the screen
        showMenu(currentFilter);
        
        // Check if the user pressed a key
        if (_kbhit()) {
            // Get the key they pressed
            int key = _getch();
            

            switch (key) {
                case '1':  // Change refresh interval
                    printf("\nEnter new refresh time in seconds: ");
                    scanf("%d", &refreshInterval);
                    printf("Refresh time set to %d seconds.\n", refreshInterval);
                    Sleep(1000);  
                    break;
                    
                case '2':  
                    showLogFile();
                    lastUpdateTime = 0;  // refresh  
                    break;
                    
                case '3':  // Exit program
                    printf("\nExiting program. Goodbye!\n");
                    WSACleanup();
                    return 0;
                    
                case 'f': case 'F':  // Change filter
                    currentFilter = (FilterType)((currentFilter + 1) % 7);
                    lastUpdateTime = 0;  
                    break;
                    
                case 'h': case 'H':  //help screen
                    showHelpScreen = !showHelpScreen;
                    lastUpdateTime = 0; 
                    break;
                case 's': case 'S':  // Toggle statistics
                    showStats = !showStats;
                    lastUpdateTime = 0;  //refresh
                    break;
                    
                case 'r': case 'R': case 'c': case 'C':  
                    lastUpdateTime = 0;  
                    break;
            }
        }
        
                Sleep(100);  // Small delay to prevent high CPU usage
    }
    
   
    WSACleanup();
    return 0;
}

// Get the name of the current filter
const char* getFilterName(FilterType filter) {
    switch (filter) {
        case FILTER_ALL: return "ALL";
        case FILTER_LOCAL: return "LOCAL";
        case FILTER_INTERNET: return "INTERNET";
        case FILTER_ACTIVE: return "ACTIVE";
        case FILTER_LISTENING: return "LISTENING";
        case FILTER_TCP: return "TCP";
        case FILTER_UDP: return "UDP";
        default: return "UNKNOWN";
    }
}

// Clear the console screen
void clearConsoleScreen() {
    system("cls"); 
}

// Display the program title
void showHeader() {
    printf("\033[1;36m");  // Cyan colour
    printf("======================================================================\n");
    printf("                  NETWORK MONITORING TOOL\n");
    printf("======================================================================\n");
    printf("\033[0m");  // Reset colour
}

// Display help information
void showHelp() {
    printf("\n\033[1;33mHow to use this program:\033[0m\n");
    printf("  [1] Change how often the program updates (in seconds)\n");
    printf("  [2] View saved connection logs\n");
    printf("  [3] Exit the program\n");
    printf("  [F] Change what connections to show\n");
    printf("  [S] Show/hide connection statistics\n");
    printf("  [H] Show/hide this help screen\n");
    printf("  [R] Refresh the screen now\n\n");
    
    printf("\033[1;33mTypes of connections you can filter:\033[0m\n");
    printf("  ALL: Show all connections\n");
    printf("  LOCAL: Show only connections to your own computer (127.0.0.1)\n");
    printf("  INTERNET: Show only connections to other computers\n");
    printf("  ACTIVE: Show only established connections\n");
    printf("  LISTENING: Show only listening ports (waiting for connections)\n");
    printf("  TCP: Show only TCP connections\n");
    printf("  UDP: Show only UDP connections\n");
}

// Display connection statistics
void showStatistics(NetworkConnection connectionList[], int connectionCount) {
    // Variables for counting each type of connection
    int activeCount = 0;
    int listeningCount = 0;
    int localCount = 0;
    int internetCount = 0;
    int tcpCount = 0;
    int udpCount = 0;
    
    // Count each type of connection
    for (int i = 0; i < connectionCount; i++) {
        // Count by state
        if (strcmp(connectionList[i].state, "ESTABLISHED") == 0) activeCount++;
        if (strcmp(connectionList[i].state, "LISTENING") == 0) listeningCount++;
        
        // Count by address
        if (strncmp(connectionList[i].localAddress, "127.", 4) == 0) localCount++;
        else internetCount++;
        
        // Count by protocol
        if (strcmp(connectionList[i].protocol, "TCP") == 0) tcpCount++;
        if (strcmp(connectionList[i].protocol, "UDP") == 0) udpCount++;
    }
    
    // Display statistics
    printf("\n\033[1;32mConnection Statistics:\033[0m\n");
    printf("  Total Connections: %d\n", connectionCount);
    printf("  Active: %d | Listening: %d\n", activeCount, listeningCount);
    printf("  Local: %d | Internet: %d\n", localCount, internetCount);
    printf("  TCP: %d | UDP: %d\n", tcpCount, udpCount);
}

// Get all network connections (both TCP and UDP)
void getNetworkConnections(NetworkConnection connectionList[], int *connectionCount) {
    *connectionCount = 0;  
       
    getTcpConnections(connectionList, connectionCount);


    getUdpConnections(connectionList, connectionCount);
}

// getting TCP information
void getTcpConnections(NetworkConnection connectionList[], int *connectionCount) {
    // to store all TCP connections
    MIB_TCPTABLE* tcpTable;
    DWORD bufferSize = 0;
    
  
    //  GetTcpTable with NULL tells required memory/buffer size
    if (GetTcpTable(NULL, &bufferSize, TRUE) != ERROR_INSUFFICIENT_BUFFER) {
        printf("Error: Could not get TCP connection information\n");
        return; 
    }
    
    tcpTable = (MIB_TCPTABLE*)malloc(bufferSize);  // Allocate memory
    if (tcpTable == NULL) {
        printf("Error: Out of memory\n"); 
        return;
    }
    
    //fetching  TCP connection table
    if (GetTcpTable(tcpTable, &bufferSize, TRUE) != NO_ERROR) {
        printf("Error: Could not get TCP connection information\n");
        free(tcpTable); 
        return;
    }
    

    for (DWORD i = 0; i < tcpTable->dwNumEntries && *connectionCount < MAX_CONNECTIONS; i++) {
        MIB_TCPROW* row = &tcpTable->table[i];
        NetworkConnection* connection = &connectionList[*connectionCount]; 
        
        // Get local address and port 
        struct in_addr addr;
        addr.s_addr = row->dwLocalAddr;
        strcpy(connection->localAddress, inet_ntoa(addr));  // Convert to text form
        connection->localPort = ntohs((u_short)row->dwLocalPort);  // Get port number
        
        // Get remote address and port
        addr.s_addr = row->dwRemoteAddr;
        strcpy(connection->remoteAddress, inet_ntoa(addr)); 
        connection->remotePort = ntohs((u_short)row->dwRemotePort); 
       

        strcpy(connection->protocol, "TCP");
        strcpy(connection->state, getConnectionStateName(row->dwState));
        

        connection->programId = 0;
        strcpy(connection->programName, "Unknown");
        

        (*connectionCount)++;
    }
    free(tcpTable);
}

// getting UDP connection information
void getUdpConnections(NetworkConnection connectionList[], int *connectionCount) {

    MIB_UDPTABLE* udpTable;
    DWORD bufferSize = 0;
    
    if (GetUdpTable(NULL, &bufferSize, TRUE) != ERROR_INSUFFICIENT_BUFFER) {
        printf("Error: Could not get UDP connection information\n");
        return;  // Stop if there's an error
    }
    
 
    udpTable = (MIB_UDPTABLE*)malloc(bufferSize);  // Get the memory
    if (udpTable == NULL) {
        printf("Error: Out of memory\n");
        return;  // Stop here
    }
    

    if (GetUdpTable(udpTable, &bufferSize, TRUE) != NO_ERROR) {
        printf("Error: Could not get UDP connection information\n");
        free(udpTable);
        return;
    }
    

    for (DWORD i = 0; i < udpTable->dwNumEntries && *connectionCount < MAX_CONNECTIONS; i++) {
        MIB_UDPROW* row = &udpTable->table[i];
        NetworkConnection* connection = &connectionList[*connectionCount];
        
        // Get local address and port
        struct in_addr addr;
        addr.s_addr = row->dwLocalAddr;
        strcpy(connection->localAddress, inet_ntoa(addr));  // Convert to text form
        connection->localPort = ntohs((u_short)row->dwLocalPort);  // Get port number
        
        // UDP doesn't have a remote address because it's connectionless

        strcpy(connection->remoteAddress, "*");
        connection->remotePort = 0;
        
        // Set protocol and state
        strcpy(connection->protocol, "UDP");
        strcpy(connection->state, "LISTENING");
      

        connection->programId = 0;
        
        strcpy(connection->programName, "Unknown");
        

        switch (connection->localPort) {
            case 53:
                strcpy(connection->programName, "DNS");  // DNS server uses port 53
                break;

            case 123:
                strcpy(connection->programName, "NTP");  // Network Time Protocol uses port 123
                break;
            case 137:
            case 138:
                strcpy(connection->programName, "NetBIOS");  // NetBIOS uses ports 137 and 138
                break;
            case 161:
            case 162:
                strcpy(connection->programName, "SNMP");  // SNMP uses ports 161 and 162
                break;
        }
        
        (*connectionCount)++;
    }
   

    free(udpTable);
}

// Get program name from process ID 
void getProgramName(DWORD processId, char* nameBuffer, int maxLength) {
    // For system processes
    if (processId == 0 || processId == 4) {
        strcpy(nameBuffer, "System");
        return;
    }

    sprintf(nameBuffer, "PID: %lu", processId);
}

// Convert TCP state number to a readable name
char* getConnectionStateName(DWORD state) {
    // TCP states and their names
    switch (state) {
        case MIB_TCP_STATE_CLOSED:
            return "CLOSED";
        case MIB_TCP_STATE_LISTEN:
            return "LISTENING";
        case MIB_TCP_STATE_SYN_SENT:
            return "SYN_SENT";
        case MIB_TCP_STATE_SYN_RCVD:
            return "SYN_RCVD";
        case MIB_TCP_STATE_ESTAB:
            return "ESTABLISHED";
        case MIB_TCP_STATE_FIN_WAIT1:
            return "FIN_WAIT1";
        case MIB_TCP_STATE_FIN_WAIT2:
            return "FIN_WAIT2";
        case MIB_TCP_STATE_CLOSE_WAIT:
            return "CLOSE_WAIT";
        case MIB_TCP_STATE_CLOSING:
            return "CLOSING";
        case MIB_TCP_STATE_LAST_ACK:
            return "LAST_ACK";
        case MIB_TCP_STATE_TIME_WAIT:
            return "TIME_WAIT";
        case MIB_TCP_STATE_DELETE_TCB:
            return "DELETE_TCB";
        default:
            return "UNKNOWN";
    }
}

// Display connections with filtering
void showConnections(NetworkConnection connectionList[], int connectionCount, FilterType filter) {
    time_t currentTime = time(NULL);
    char* timeString = ctime(&currentTime);
     int shownCount = 0;
    
    printf("Network Connections as of %s", timeString);
    printf("Filter: \033[1;33m%s\033[0m\n", getFilterName(filter));
    printf("=========================================================================================\n");
        printf("\033[1;37m");  //white
    printf("%-15s | %-6s | %-15s | %-6s | %-6s | %-12s | %-20s\n", 
           "Local Address", "Port", "Remote Address", "Port", "Type", "State", "Program");
    printf("\033[0m");  //resets the text colour

    printf("-----------------------------------------------------------------------------------------\n");
    

    for (int i = 0; i < connectionCount; i++) {

        int showThisConnection = 1;
        
        // Check if this connection should be shown based on the current filter
        switch (filter) {
            case FILTER_LOCAL:
                // Show only local connections (127.0.0.1)
                showThisConnection = (strncmp(connectionList[i].localAddress, "127.", 4) == 0);
                break;
                
            case FILTER_INTERNET:
                // Show only internet connections (not 127.0.0.1)
                showThisConnection = (strncmp(connectionList[i].localAddress, "127.", 4) != 0);
                break;
                
            case FILTER_ACTIVE:
                // Show only established connections
                showThisConnection = (strcmp(connectionList[i].state, "ESTABLISHED") == 0);
                break;
                
            case FILTER_LISTENING:
                // Show only listening ports
                showThisConnection = (strcmp(connectionList[i].state, "LISTENING") == 0);
                break;
                
            case FILTER_TCP:
                // Show only TCP connections
                showThisConnection = (strcmp(connectionList[i].protocol, "TCP") == 0);
                break;
                
            case FILTER_UDP:
                // Show only UDP connections
                showThisConnection = (strcmp(connectionList[i].protocol, "UDP") == 0);
                break;
                
            default:
                // FILTER_ALL: show everything
                showThisConnection = 1;
        }
        

        if (showThisConnection) {

            
            // Yellow for UDP connections
            if (strcmp(connectionList[i].protocol, "UDP") == 0) {
                printf("\033[1;33m");  // Yellow for UDP
            } else if (strcmp(connectionList[i].state, "ESTABLISHED") == 0) {
                printf("\033[1;32m");  // Green for established connections
            }             else if (strcmp(connectionList[i].state, "LISTENING") == 0) {
                printf("\033[1;36m");  // Cyan for listening ports
            }             else {
                printf("\033[0;37m");  // Gray for other states
            }
            
            // Print the connection details
            printf("%-15s | %-6d | %-15s | %-6d | %-6s | %-12s | %-20s\n", 
                  connectionList[i].localAddress,   // Local IP address
                  connectionList[i].localPort,      // Local port
                  connectionList[i].remoteAddress,  // Remote IP address
                  connectionList[i].remotePort,     // Remote port
                  connectionList[i].protocol,       // TCP or UDP
                  connectionList[i].state,          // Connection state
                  connectionList[i].programName);   // Program name
                  
            // Reset colour back to normal
            printf("\033[0m");
            
            shownCount++;
        }
    }
    
    // Show a summary of how many connections we displayed
    printf("=========================================================================================\n");
    printf("Showing %d of %d connections\n\n", shownCount, connectionCount);
}

// Save connection information to a log file
void saveConnectionsToLog(NetworkConnection connectionList[], int connectionCount) {

    FILE *logFile = fopen(LOG_FILE, "a");

    if (logFile == NULL) {
        printf("Error: Could not open log file.\n");
        return;
    }
    

    time_t currentTime = time(NULL);
    char timeString[26];
    
    // Convert time to text and copy it to our string
    strcpy(timeString, ctime(&currentTime));
    
    // The time string ends with a newline,replace the \n with \0 
    timeString[24] = '\0';
    

    fprintf(logFile, "[%s] - %d connections\n", timeString, connectionCount);
    

    for (int i = 0; i < connectionCount; i++) {
        // Format: local_ip:port -> remote_ip:port [protocol] state (program)
        fprintf(logFile, "%s:%d -> %s:%d [%s] %s (%s)\n", 
                connectionList[i].localAddress,  // Local IP
                connectionList[i].localPort,     // Local port
                connectionList[i].remoteAddress, // Remote IP
                connectionList[i].remotePort,    // Remote port
                connectionList[i].protocol,      // TCP or UDP
                connectionList[i].state,         // Connection state
                connectionList[i].programName);  // Program name
    }
    

    fprintf(logFile, "--------------------------------\n");
    

    fclose(logFile);
}

// Display the menu options at the bottom of the screen
void showMenu(FilterType currentFilter) {

    printf("Menu: [1] Update Time | [2] View Logs | [3] Exit | [F] Filter: %s | [S] Statistics | [H] Help | [R] Refresh\n", 
           getFilterName(currentFilter));  // Show which filter is currently active
}

// View the saved log file
void showLogFile() {

    system("cls");
    printf("====== Network Connection Logs ======\n\n");
    
    FILE *logFile;
    logFile = fopen(LOG_FILE, "r");

    if (logFile == NULL) {
        printf("No logs found! Run the program for a while to make some logs!\n");
        printf("\nPress any key to go back to main screen...");
        _getch();
        return;
    }
     
    char line[256];
    

    while (fgets(line, sizeof(line), logFile) != NULL) {
        printf("%s", line);
    }
    

    fclose(logFile);
    
    printf("\nPress any key to go back to main screen...");
    _getch();
}