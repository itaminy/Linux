

#include<stdio.h>
#include<string.h>

int main(){
  printf( "$" );
  char input[ 100 ];
while(1){
        printf("%s:commane is not found ",input);
        fgets(input,sizeof(input),stdin);
        input[strcspn(input, "\n")]='\0';

        if(strcmp(input, "/q") == 0){
               printf("close...\n");
               break;
        }
	printf("\n");
}
  return 0;

}
