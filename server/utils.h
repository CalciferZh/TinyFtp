#define USER_CODE 0
#define PASS_CODE 1

#define USER_COMMAND "user"
#define PASS_COMMAND "pass"
#define XPWD_COMMAND "xpwd"


#define RES_READY              "220 Anonymous FTP server ready.\r\n"
#define RES_UNKNOWN            "500 Unknown command.\r\n"

#define RES_WANTUSER           "500 Command USER is expected.\r\n"
#define RES_ACCEPT_USER        "220 User accepted.\r\n"
#define RES_REJECT_USER        "503 Unknown user.\r\n"

#define RES_WANTPASS           "500 Command PASS is expected.\r\n"
#define RES_ACCEPT_PASS        "220 Password accepted.\r\n"
#define RES_REJECT_PASS        "503 Wrong password.\r\n"


#define USER_NAME "anonymous"

// a secured method to send message
int send_msg(int connfd, char* message);

// a secured method to receive message
int read_msg(int connfd, char* message);

void str_lower(char* str);

void split_command(char* message, char* command, char* content);

// parse the command from client
int parse_command(char* message, char* content);

int command_user(int connfd, char* uname);

void strip_crlf(char* uname);

// return 1 if password is correct
int command_pass(int connfd, char* pwd);

int command_unknown(int connfd);

int serve(int connfd);


