#include "utils.h"

int send_msg(struct ServerState* state, char* str)
{
  int p = 0;
  int len = strlen(str);
  char* message;
  if (state->encrypt) {
    encodeString(str, &message, state->priv_exp, state->priv_mod);
    len = strlen(message);
  } else {
    message = str;
  }
  while (p < len) {
    int n = write(state->command_fd, message + p, len - p);
    if (n < 0) {
      sprintf(error_buf, ERROR_PATT, "write", "send_msg");
      perror(error_buf);
      return -1;
    } else {
      p += n;
    }     
  }
  if (state->encrypt) {
    free(message);
  }
  return 0;
}

int send_file(int des_fd, int src_fd, int offset)
{
  printf("preparing to send file in single-thread mode...\n");

  struct stat stat_buf;
  fstat(src_fd, &stat_buf);
  lseek(src_fd, offset, SEEK_SET);

  int to_read, fin_read;
  char buf[DATA_BUF_SIZE];

  int remain = stat_buf.st_size - offset;
  printf("Start file transfer...\n");
  printf("%d bytes to send...\n", remain);

  while (remain > 0) {
    to_read = remain < DATA_BUF_SIZE ? remain : DATA_BUF_SIZE;
    fin_read = read(src_fd, buf, to_read);
    if (fin_read < 0) {
      sprintf(error_buf, ERROR_PATT, "read", "send_file");
      perror(error_buf);
      return -1;
    }
    if (write(des_fd, buf, fin_read) == -1) {
      sprintf(error_buf, ERROR_PATT, "write", "send_file");
      perror(error_buf);
      return -1;
    }
    remain -= fin_read;
  }
  printf("Transfer success!\n");
  return 0;
}

void write_thread(void* arg)
{
  struct write_para* para = (struct write_para*)arg;
  write(para->des_fd, para->buf, para->size);
}

int send_file_mt(int des_fd, int src_fd, int offset)
{
  printf("preparing to send file in multi-thread mode...\n");

  struct stat stat_buf;
  fstat(src_fd, &stat_buf);
  lseek(src_fd, offset, SEEK_SET);

  int to_read;
  int fin_read_1;
  int fin_read_2;
  char buf_1[DATA_BUF_SIZE_LARGE];
  char buf_2[DATA_BUF_SIZE_LARGE];

  int remain = stat_buf.st_size - offset;

  printf("Start file transfer...\n");
  printf("%d bytes to send...\n", remain);

  pthread_t pid;

  int ret;

  struct write_para arg;

  to_read = remain < DATA_BUF_SIZE ? remain : DATA_BUF_SIZE;
  fin_read_1 = read(src_fd, buf_1, to_read);
  if (fin_read_1 < 0) {
    sprintf(error_buf, ERROR_PATT, "read", "send_file");
    perror(error_buf);
    return -1;
  }
  remain -= fin_read_1;

  arg.des_fd = des_fd;
  // at least run once to sendout buf_1
  while (remain > 0) {
    // new thread to send buf_1
    arg.buf = buf_1;
    arg.size = fin_read_1;
    ret = pthread_create(&pid, NULL, (void*)write_thread, (void *)&arg);
    if (ret) {
      sprintf(error_buf, ERROR_PATT, "pthread_create", "send_file_mt");
      perror(error_buf);
      return -1;
    }

    // at the same time fill buf_2
    to_read = remain < DATA_BUF_SIZE ? remain : DATA_BUF_SIZE;
    fin_read_2 = read(src_fd, buf_2, to_read);
    if (fin_read_2 < 0) {
      sprintf(error_buf, ERROR_PATT, "read", "send_file_mt");
      perror(error_buf);
      return -1;
    }
    remain -= fin_read_2;

    // sync
    pthread_join(pid, NULL);

    // new thread to send buf_2
    arg.buf = buf_2;
    arg.size = fin_read_2;
    ret = pthread_create(&pid, NULL, (void*)write_thread, (void *)&arg);
    if (ret) {
      sprintf(error_buf, ERROR_PATT, "pthread_create", "send_file_mt");
      perror(error_buf);
      return -1;
    }

    // at the same time fill buf_1
    to_read = remain < DATA_BUF_SIZE ? remain : DATA_BUF_SIZE;
    fin_read_1 = read(src_fd, buf_1, to_read);
    if (fin_read_1 < 0) {
      sprintf(error_buf, ERROR_PATT, "read", "send_file_mt");
      perror(error_buf);
      return -1;
    }
    remain -= fin_read_1;

    // sync
    pthread_join(pid, NULL);
  }
  write(des_fd, buf_1, fin_read_1);

  printf("Transfer success!\n");
  return 0;
}

int recv_file(int des_fd, int src_fd)
{
  int len;
  char buf[DATA_BUF_SIZE];

  while ((len = read(src_fd, buf, DATA_BUF_SIZE)) > 0) {
    if (write(des_fd, buf, len) == -1) {
      sprintf(error_buf, ERROR_PATT, "write", "recv_file");
      perror(error_buf);
      return -1;
    }
  }

  if (len == 0) {
    return 1;
  } else {
    sprintf(error_buf, ERROR_PATT, "read", "recv_file");
    perror(error_buf);
    return -1;
  }
}

int read_msg(struct ServerState* state, char* str)
{
  int n = read(state->command_fd, str, DATA_BUF_SIZE);
  str[n] = '\0';

  if (n < 0) {
    sprintf(error_buf, ERROR_PATT, "read", "read_msg");
    perror(error_buf);
    close(state->command_fd);
    return -1;
  }

  if (state->encrypt) {
    char* message;
    decodeString(str, &message, state->priv_exp, state->priv_mod);
    printf("%s\n", message);
    strcpy(str, message);
    free(message);
  }

  n = strlen(str);
  str[n] = '\0';
  return n;
}

void str_lower(char* str)
{
  int p = 0;
  int len = strlen(str);
  for (p = 0; p < len; p++) {
    str[p] = tolower(str[p]);
  }
}

void str_replace(char* str, char src, char des)
{
  char* p = str;
  while (1) {
    p = strchr(p, src);
    if (p) {
      *p = des;
    } else {
      break;
    }
  }
}

void split_command(char* message, char* command, char* content)
{
  char* blank = strchr(message, ' ');

  if (blank != NULL) {
    strncpy(command, message, (int)(blank - message));
    command[(int)(blank - message)] = '\0';
    strcpy(content, blank + 1);
  } else {
    strcpy(command, message);
    content[0] = '\0';
  }
}

int parse_command(char* message, char* content)
{
  char command[16]; // actually all commands are 4 bytes or less
  split_command(message, command, content);
  strip_crlf(command);
  strip_crlf(content);
  str_lower(command);

  int ret = -1;

  if (strcmp(command, USER_COMMAND) == 0) {
    ret = USER_CODE;
  } 
  else if (strcmp(command, PASS_COMMAND) == 0) {
    ret = PASS_CODE;
  }
  else if (strcmp(command, XPWD_COMMAND) == 0) {
    ret = XPWD_CODE;
  }
  else if (strcmp(command, QUIT_COMMAND) == 0) {
    ret = QUIT_CODE;
  }
  else if (strcmp(command, PORT_COMMAND) == 0) {
    ret = PORT_CODE;
  }
  else if (strcmp(command, PASV_COMMAND) == 0) {
    ret = PASV_CODE;
  }
  else if (strcmp(command, RETR_COMMAND) == 0) {
    ret = RETR_CODE;
  }
  else if (strcmp(command, SYST_COMMAND) == 0) {
    ret = SYST_CODE;
  }
  else if (strcmp(command, STOR_COMMAND) == 0) {
    ret = STOR_CODE;
  }
  else if (strcmp(command, TYPE_COMMAND) == 0) {
    ret = TYPE_CODE;
  }
  else if (strcmp(command, ABOR_COMMAND) == 0) {
    ret = ABOR_CODE;
  }
  else if (strcmp(command, LIST_COMMAND) == 0) {
    ret = LIST_CODE;
  }
  else if (strcmp(command, NLST_COMMAND) == 0) {
    ret = NLST_CODE;
  }
  else if (strcmp(command, MKD_COMMAND) == 0) {
    ret = MKD_CODE;
  }
  else if (strcmp(command, CWD_COMMAND) == 0) {
    ret = CWD_CODE;
  }
  else if (strcmp(command, RMD_COMMAND) == 0) {
    ret = RMD_CODE;
  }
  else if (strcmp(command, REST_COMMAND) == 0) {
    ret = REST_CODE;
  }
  else if (strcmp(command, MULT_COMMAND) == 0) {
    ret = MULT_CODE;
  }
  else if (strcmp(command, ENCR_COMMAND) == 0) {
    ret = ENCR_CODE;
  }
  return ret;
}

int connect_by_mode(struct ServerState* state)
{
  if (state->trans_mode == PORT_CODE) {
    if (connect(
          state->data_fd,
          (struct sockaddr*)&(state->target_addr),
          sizeof(state->target_addr)
        ) < 0) {
      sprintf(error_buf, ERROR_PATT, "connect", "command_stor");
      perror(error_buf);
      send_msg(state, RES_FAILED_CONN);
      return -1;
    }
  } else if (state->trans_mode == PASV_CODE){
    if ((state->data_fd = accept(state->listen_fd, NULL, NULL)) == -1) {
      sprintf(error_buf, ERROR_PATT, "accept", "command_stor");
      perror(error_buf);
      send_msg(state, RES_FAILED_LSTN);
      return -1;
    }
    close(state->listen_fd);
  } else {
    send_msg(state, RES_WANTCONN);
    return -1;
  }
  return 0;
}

int close_connections(struct ServerState* state)
{
  close(state->data_fd);
  state->data_fd = -1;
  state->trans_mode = -1;
  return 0;
}

int parse_addr(char* content, char* ip)
{
  str_replace(content, ',', '.');

  int i = 0;
  char* dot = content;
  for (i = 0; i < 4; ++i) {
    dot = strchr(++dot, '.');
  }

  // retrieve ip address
  strncpy(ip, content, (int)(dot - content));
  strcat(ip, "\0");

  // retrieve port 1
  ++dot;
  char* dot2 = strchr(dot, '.');
  char buf[32];
  strncpy(buf, dot, (int)(dot2 - dot));
  strcat(buf, "\0");
  int p1 = atoi(buf);

  //retrieve port 2
  int p2 = atoi(strcpy(buf, dot2 + 1));

  return (p1 * 256 + p2);
  // return 0;
}

int parse_argv(int argc, char** argv, char* hip, char* hport, char* root)
{
  hip[0] = '\0';
  hport[0] = '\0';
  root[0] = '\0';
  struct option opts[] = {
    {"ip-address", required_argument, NULL, 'a'},
    {"port",       optional_argument, NULL, 'p'},
    {"root",       optional_argument, NULL, 'r'}
  };
  int opt;
  while ((opt = getopt_long(argc, argv, "a:p:r:", opts, NULL)) != -1) {
    switch (opt) {
      case 'a':
        strcpy(hip, optarg);
        break;

      case 'p':
        strcpy(hport, optarg);
        break;

      case 'r':
        strcpy(root, optarg);
        break;

      case '?':
       printf("Unknown option: %c\n", (char)optopt);
       break;

      default:
        sprintf(error_buf, ERROR_PATT, "getopt_long", "parse_argv");
        perror(error_buf);
        break;
    }
  }

  if (strlen(hport) == 0) {
    strcpy(hport, "21");
  }

  if (strlen(root) == 0) {
    strcpy(root, "/tmp");
  }

  return 0;
}

void strip_crlf(char* str)
{
  int len = strlen(str);
  if (len < 1) {
    return;
  }
  if (str[len - 1] == '\n' || str[len - 1] == '\r') {
    str[len - 1] = '\0';
    if (len < 2) {
      return;
    }
    if (str[len - 2] == '\n' || str[len - 2] == '\r') {
      str[len - 2] = '\0';
    }
  }
}

int command_user(struct ServerState* state, char* uname)
{
  int ret = 0;
  //if (strcmp(uname, USER_NAME) == 0) {
  if (1) {
    send_msg(state, RES_ACCEPT_USER);
    ret = 1;
  } else {
    send_msg(state, RES_REJECT_USER);
  }

  return ret;
}

int command_pass(struct ServerState* state, char* pwd)
{
  //if (strcmp(pwd, PASSWORD) == 0) {
  if (1) {
    send_msg(state, RES_ACCEPT_PASS);
    state->logged = 1;
  } else {
    send_msg(state, RES_REJECT_PASS);
    state->logged = 0;
  }

  return state->logged;
}

int command_unknown(struct ServerState* state)
{
  send_msg(state, RES_UNKNOWN);
  return 0;
}

int command_port(struct ServerState* state, char* content)
{
  struct sockaddr_in* addr = &(state->target_addr);

  if ((state->data_fd = socket(AF_INET, SOCK_STREAM,  IPPROTO_TCP)) == -1) {
    sprintf(error_buf, ERROR_PATT, "scoket", "aommand_port");
    perror(error_buf);
    send_msg(state, RES_REJECT_PORT);
    return -1;
  }

  // check
  if (!strlen(content)) {
    send_msg(state, RES_ACCEPT_PORT);
    return 1;
  }

  char ip[64];
  int port = parse_addr(content, ip);
  memset(addr, 0, sizeof(*addr));
  addr->sin_family = AF_INET;
  addr->sin_port = htons(port);

  // translate the decimal IP address to binary
  if (inet_pton(AF_INET, ip, &(addr->sin_addr)) <= 0) {
    sprintf(error_buf, ERROR_PATT, "inet_pton", "command_port");
    perror(error_buf);
    send_msg(state, RES_REJECT_PORT);
    return -1;
  }

  send_msg(state, RES_ACCEPT_PORT);
  state->trans_mode = PORT_CODE;
  return 1;
}

int command_pasv(struct ServerState* state)
{
  char* hip = state->hip;
  struct sockaddr_in* addr = &(state->target_addr);

  if ((state->listen_fd = socket(AF_INET, SOCK_STREAM,  IPPROTO_TCP)) == -1) {
    sprintf(error_buf, ERROR_PATT, "scoket", "command_pasv");
    perror(error_buf);
    send_msg(state, RES_REJECT_PASV);
    return -1;
  }

  int p1, p2;
  int port = get_random_port(&p1, &p2);

  memset(addr, 0, sizeof(*addr));
  addr->sin_family = AF_INET;
  addr->sin_port = htons(port);
  addr->sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(state->listen_fd, (struct sockaddr*)addr, sizeof(*addr)) == -1) {
    sprintf(error_buf, ERROR_PATT, "bind", "command_pasv");
    perror(error_buf);
    send_msg(state, RES_REJECT_PASV);
    return -1;
  }

  if (listen(state->listen_fd, 10) == -1) {
    sprintf(error_buf, ERROR_PATT, "listen", "command_pasv");
   perror(error_buf);
    send_msg(state, RES_REJECT_PASV);
    return -1;
  }

  char hip_cp[32] = "";
  strcpy(hip_cp, hip);
  str_replace(hip_cp, '.', ',');
  char buffer[32] = "";
  sprintf(buffer, RES_ACCEPT_PASV, hip_cp, p1, p2);
  send_msg(state, buffer);
  state->trans_mode = PASV_CODE;

  return 1;
}

int command_quit(struct ServerState* state)
{
  send_msg(state, RES_CLOSE);
  close(state->command_fd);
  return 0;
}

int command_retr(struct ServerState* state, char* path)
{
  if (access(path, 4) != 0) { // 4: readable
    send_msg(state, RES_TRANS_NOFILE);
    return -1;
  }

  int src_fd;
  if ((src_fd = open(path, O_RDONLY)) == 0) {
    send_msg(state, RES_TRANS_NREAD);
    return -1;
  }

  if (connect_by_mode(state) != 0) {
    return -1;
  }

  send_msg(state, RES_TRANS_START);

  int flag;
  struct timespec t1, t2;
  clock_gettime(CLOCK_MONOTONIC, &t1);
  if (state->thread == 1) {
    flag = send_file(state->data_fd, src_fd, state->offset);
  } else {
    flag = send_file_mt(state->data_fd, src_fd, state->offset);
  }
  clock_gettime(CLOCK_MONOTONIC, &t2);
  int delta = (int)((t2.tv_sec - t1.tv_sec) * 1000 + (t2.tv_nsec - t1.tv_nsec) / 1000000);

  printf("finish sending, time cost %dms\n", delta);

  if (flag == 0) {
    send_msg(state, RES_TRANS_SUCCESS);
  } else {
    send_msg(state, RES_TRANS_FAIL);
  }

  close_connections(state);
  state->offset = 0;
  return 0;
}

int command_stor(struct ServerState* state, char* path)
{
  int des_fd;
  if ((des_fd = open(path, O_WRONLY | O_CREAT)) == 0) {
    send_msg(state, RES_TRANS_NCREATE);
    return -1;
  }

  if (connect_by_mode(state) != 0) {
    return -1;
  }

  send_msg(state, RES_TRANS_START);
  if (recv_file(des_fd, state->data_fd) == 0) {
    send_msg(state, RES_TRANS_SUCCESS);
  } else {
    send_msg(state, RES_TRANS_FAIL);
  }

  close_connections(state);
  return 0;
}

int command_type(struct ServerState* state, char* content)
{
  str_lower(content);
  if (content[0] == 'i' || content[0] == 'l') {
    state->binary_flag = 1;
    send_msg(state, RES_ACCEPT_TYPE);
  } else if (content[0] == 'a') {
    state->binary_flag = 0;
    send_msg(state, RES_ACCEPT_TYPE);
  } else {
    send_msg(state, RES_ERROR_ARGV);
  }
  return 0;
}

int command_list(struct ServerState* state, char* path, int is_long)
{
  if (strlen(path) == 0) {
    path = "./";
  }
  
  if (access(path, 0) != 0) { // 0: existence
    send_msg(state, RES_TRANS_NOFILE);
    return -1;
  }

  char cmd[128];
  if (is_long) {
    sprintf(cmd, "ls -l %s", path);
  } else {
    sprintf(cmd, "ls %s", path);
  }
  
  FILE* fp = popen(cmd, "r");

  if (!fp) {
    sprintf(error_buf, ERROR_PATT, "popen", "command_list");
    perror(error_buf);
    send_msg(state, RES_TRANS_NREAD);
  }

  if (connect_by_mode(state) != 0) {
    return -1;
  }

  send_msg(state, RES_TRANS_START);

  char buf[256];
  if (is_long) {
    fgets(buf, sizeof(buf), fp); // remove the first line
  }
  while (fgets(buf, sizeof(buf), fp)) {
    strip_crlf(buf);
    strcat(buf, "\r\n");
    // printf("%s", buf);
    write(state->data_fd, buf, strlen(buf));
  }

  printf("Command list finish transfer.\n");
  send_msg(state, RES_TRANS_SUCCESS);
  pclose(fp);
  close_connections(state);
  return 0;
}

int command_mkd(struct ServerState* state, char* path)
{
  int flag = mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  if (flag == 0) {
    send_msg(state, RES_ACCEPT_MKD);
  } else {
    send_msg(state, RES_REJECT_MKD);
  }
  return flag;
}

int command_cwd(struct ServerState* state, char* path)
{
  if (chdir(path) == -1) {
    send_msg(state, RES_REJECT_CWD); 
  } else {
    send_msg(state, RES_ACCEPT_CWD);
  }
  return 0;
}

int command_rmd(struct ServerState* state, char* path)
{
  char cmd[32] = "rm -rf ";
  strcat(cmd, path);
  if (system(cmd) == 0) {
    send_msg(state, RES_ACCEPT_RMD);
  } else {
    send_msg(state, RES_REJECT_RMD);
  }
  return 0;
}

int command_rest(struct ServerState* state, char* content)
{
  state->offset = atoi(content);
  if (state->offset > 0) {
    send_msg(state, RES_ACCEPT_REST);
  } else {
    state->offset = 0;
    send_msg(state, RES_REJECT_REST);
  }
  return 0;
}

int command_mult(struct ServerState* state)
{
  if (state->thread == 1) {
    state->thread = 2;
    send_msg(state, RES_MULTIT_ON);
  } else {
    state->thread = 1;
    send_msg(state, RES_MULTIT_OFF);
  }
  return 0;
}

int command_encr(struct ServerState* state)
{
  if (state->encrypt == 1) {
    state->encrypt = 0;
    send_msg(state, RES_ENCR_OFF);
    bignum_deinit(state->pub_exp);
    bignum_deinit(state->pub_mod);
    bignum_deinit(state->priv_exp);
    bignum_deinit(state->priv_mod);
    state->pub_exp = NULL;
    state->pub_mod = NULL;
    state->priv_exp = NULL;
    state->priv_mod = NULL;
  } else {
    gen_rsa_key(&(state->pub_exp), &(state->pub_mod),
      &(state->priv_exp), &(state->priv_mod), &(state->bytes));

    char* pub_exp = bignum_tostring(state->pub_exp);
    char* pub_mod = bignum_tostring(state->pub_mod);
    char buf[1024];
    sprintf(buf, RES_ENCR_ON, pub_exp, pub_mod, state->bytes);
    send_msg(state, buf);
    free(pub_exp);
    free(pub_mod);

    state->encrypt = 1;
    send_msg(state, "200 Hello, this is an encrypted message.");
  }
  return 0;
}

int get_random_port(int* p1, int* p2)
{
  int port = rand() % (65535 - 20000) + 20000;
  *p1 = port / 256;
  *p2 = port % 256;
  return port;
}



