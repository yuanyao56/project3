int
main(){
       	char a[4096*20];
        int i = 0;
        int status = 0;
        for (i = 0; i < 4096*20; i++){
                a[i] = 1;
        }

	int b = fork();
        if(b==0){
                wait(&status);
        }
	else{
             	for(i=0;i<4096*20;i++){
                        cprintf(&a[i]);
                }
                return 0;
        }
        return 0;
}

