
#define CERVENA     "\033[1;31m"
#define ZELENA      "\033[1;32m"
#define ZLUTA       "\033[1;33m"
#define MODRA       "\033[1;34m"
#define NACHOVA     "\033[1;35m"
#define TYRKYSOVA   "\033[1;36m"

#define ZADNA       "\033[0m"

char radky[16][30] = {
    "                           \r\n",
    "     _.-^^---....,,--      \r\n",
    " _--                  --_  \r\n",
    "<                        >)\r\n",
    "|                         |\r\n",
    " \\._                   _./\r\n",
    "    ```--. . , ; .--'''    \r\n",
    "          | |   |          \r\n",
    "      .-=||  | |=-.        \r\n",
    "       `-=#$@&@$#=-'       \r\n",
    "          | ;  :|          \r\n", 
    "      .,-#@&$@@#&#~,.      \r\n",
    "---------------------------\r\n",
    "                           \r\n",
    "      B U M P R A S K      \r\n",
    "                           \r\n",
 };

 void animujVybuch(){
    int pocetRadku = sizeof(radky) / strlen(radky[0]);
    for(int i=(pocetRadku-1); i>=0; i--){
        smazatObrazovku;
        for(int y=0; y<i; y++){
            printf("\r\n");
        }
        for(int z=i; z<pocetRadku; z++){
            printf("%s%s%s", ZLUTA, radky[z], ZADNA);
        }
        pockejMs(100);
     }
 }