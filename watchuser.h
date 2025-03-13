struct UserNode { 
  char* user;
  struct UserNode *next;
  int isLoggedOn; 
};

struct UserNode* userAppend(struct UserNode*, char*);

struct UserNode* findUser(struct UserNode*, char*);

struct UserNode* removeUserNode(struct UserNode*, char*);

void freeUserNode(struct UserNode*);

void userFreeAll(struct UserNode*); 
