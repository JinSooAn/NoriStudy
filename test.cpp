#include<stdio.h>
#include<stdlib.h>
#include<time.h>

int main()
{
	srand(time(NULL));
	
	int numbers[45];
	for(int i = 0; i < 45; i++) {
		numbers[i] = i + 1;
	}
	
	// Fisher-Yates 셔플
	for(int i = 44; i > 0; i--) {
		int j = rand() % (i + 1);
		int temp = numbers[i];
		numbers[i] = numbers[j];
		numbers[j] = temp;
	}
	
	// 앞의 6개 정렬해서 출력
	int result[6];
	for(int i = 0; i < 6; i++) {
		result[i] = numbers[i];
	}
	
	// 오름차순 정렬
	for(int i = 0; i < 5; i++) {
		for(int j = i + 1; j < 6; j++) {
			if(result[i] > result[j]) {
				int temp = result[i];
				result[i] = result[j];
				result[j] = temp;
			}
		}
	}
	
	printf("로또 번호: ");
	for(int i = 0; i < 6; i++) {
		printf("%d ", result[i]);
	}
	printf("\n");
	
	return 0;
}
