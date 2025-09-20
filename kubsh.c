

#include<stdio.h>
#include<string.h>

int main(){
  char input[ 100 ];
	while(1){
	if (fgets(input, 99, stdin) == NULL){
		printf("Error");
		return 1;
	}
        if(strstr(input,"/q") == NULL){
        	input[strcspn(input, "\n")] = '\0';
                printf("%s: command is not found\n",input);
        }
        else{
     	       printf("\nExit\n");
               break;
	}
     }

  return 0;
}
