/**Module: rshutdown
  *@author Michael Reitgruber
  *@brief Simulates a reactor shutdown
  *@date 11.11.2015
  */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char *argv[])
{
    int r = 0;
    srand(time(NULL));
    r = rand()%3;
    if(r == 2) {
        (void)printf("SHUTDOWN COMPLETED");
        return EXIT_SUCCESS;
    }
    (void)printf("KaBOOM!");
    return EXIT_FAILURE;
}
