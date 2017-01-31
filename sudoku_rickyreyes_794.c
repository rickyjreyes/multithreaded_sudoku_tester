#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>

#define GRID_SIZE 9

// Row, Column, and Square Struct
typedef struct{

	int row;
	int column;
	int (*square)[GRID_SIZE];

} parameters;

// Initialize Coordinates of Struct Parameters
void initialize_row_column_square(parameters *data[GRID_SIZE], int sudoku_puzzle[GRID_SIZE][GRID_SIZE]);

// Thread Functions
void *thread_row_function(void *temp_param);
void *thread_column_function(void *temp_param);
void *thread_square_function(void *temp_param);

// Display Sudoku Puzzle
void display_sudoku_puzzle(int sudoku_puzzle[GRID_SIZE][GRID_SIZE]);

// Is the Sudoku Puzzle Solved?
bool isSolved(bool solved, void *row, void *column, void *square[GRID_SIZE]);

int main(void){

	// Sudoku Puzzle
	int sudoku_puzzle[GRID_SIZE][GRID_SIZE] = {
		{ 6, 5, 3, 1, 2, 8, 7, 9, 4 },
		{ 1, 7, 4, 3, 5, 9, 6, 8, 2 },
		{ 9, 2, 8, 4, 6, 7, 5, 3, 1 },
		{ 2, 8, 6, 5, 1, 4, 3, 7, 9 },
		{ 3, 9, 1, 7, 8, 2, 4, 5, 6 },
		{ 5, 4, 7, 6, 9, 3, 2, 1, 8 },
		{ 8, 6, 5, 2, 3, 1, 9, 4, 7 },
		{ 4, 1, 2, 9, 7, 5, 8, 6, 3 },
		{ 7, 3, 9, 8, 4, 6, 1, 2, 5 } };

	// Initialize Data temp_param
	parameters *data[GRID_SIZE];

	// Initialize Threads
	pthread_t thread_row;
	pthread_t thread_column; 
	pthread_t thread_squares[GRID_SIZE];

	// Initialize Thread Returns
	void *return_rows;
	void *return_columns;
	void *return_squares[GRID_SIZE];

	// Initialize Solved
	bool solved = false;

	// Initialize Coordinates of Struct Parameters
	initialize_row_column_square(data, sudoku_puzzle);

	// Create Threads Row, Column, Squares
	pthread_create(&thread_row, NULL, thread_row_function, (void *)data[0]);
	pthread_create(&thread_column , NULL, thread_column_function, (void *)data[0]);
	for(int i=0; i < GRID_SIZE; i++)
		pthread_create(&thread_squares[i], NULL, thread_square_function, (void *)data[i]);

	// Syncronize Threads Row, Column and Squares
	pthread_join(thread_row, &return_rows);
	pthread_join(thread_column, &return_columns);
	for(int i = 0; i<GRID_SIZE; i++)
		pthread_join(thread_squares[i], &return_squares[i]);

	// Display Sudoku Puzzle
	display_sudoku_puzzle(sudoku_puzzle);
 
	// Is Sudoku Puzzle Solved Or Not?
    solved = isSolved(solved, return_rows, return_columns, return_squares);

	if (solved)
		printf("\nCONGRATULATIONS!!! The given Sudoku Puzzle CORRECT!\n");
	else 
		printf("\nPLEASE, TRY AGAIN!!! The given Sudoku Puzzle INCORRECT.\n");

	return 0;
}


// Parameters for Rows, Columns, and 3X3 threads
/* Visual Square 1-9
	1 | 2 | 3  (0,0)(0,3)(0,6)
	---------
	4 | 5 | 6  (3,0)(3,3)(3,6)
	---------
	7 | 8 | 9  (6,0)(6,3)(6,6)
*/
void initialize_row_column_square(parameters *data[GRID_SIZE], int sudoku_puzzle[GRID_SIZE][GRID_SIZE]){
		for(int i = 0; i <= GRID_SIZE; i++){
		data[i] = (parameters *)malloc(sizeof(parameters));

		// Initialize Row, Column, and First Square
		data[i]->row = 0;
		data[i]->column = 0;

		// Second Row Squares
		if (i==4 || i==5 || i==6)
			data[i]->row = 3;

		// Third Row Squares
		if (i==7 || i==8 || i==9)
			data[i]->row = 6;

		// Second Column Squares
		if (i==2 || i==5 || i==8)
			data[i]->column = 3;

		// Third Column Squares
		if (i==3 || i==6 || i==9)
			data[i]->column = 6;

		// Initialize Square 1-9
		data[i]->square = sudoku_puzzle;
	}
}

// Thread Checks Each Row
void * thread_row_function(void *temp_param) {
	parameters *data = (parameters *)temp_param;

	for (int i = 0; i < GRID_SIZE; i++) {

		bool check_row[GRID_SIZE + 1] = { false };
		for (int j = 0; j < GRID_SIZE; j++) {

			int current_location = data->square[i][j];
			if (check_row[current_location] == true)
				return (void *)false;
			else
				check_row[current_location] = true;
		}
	}

	return (void *)true;
}

// Thread Checks Each Column
void * thread_column_function(void *temp_param) {
	parameters *data = (parameters *)temp_param;

	for (int i = 0; i < GRID_SIZE; i++) {

		bool check_column[GRID_SIZE + 1] = { false };
		for (int j = 0; j < GRID_SIZE; j++) {

			int current_location = data->square[j][i];
			if (check_column[current_location] == true)
				return (void *)false;
			else 
				check_column[current_location] = true;
		}
	}

	return (void *)true;
}

// Thread checks each Square
void * thread_square_function(void *temp_param) {

	parameters *data = (parameters *)temp_param;
	int temp_row = data->row;
	int temp_column = data->column;
	bool check_square[GRID_SIZE + 1] = { false };

	for (int i = temp_row; i < temp_row + 3; i++) {
		for (int j = temp_column; j < temp_column + 3; j++) {
			int current_location = data->square[i][j];
			if (check_square[current_location] == true)
				return (void *)false;
			else 
				check_square[current_location] = true;
		}
	}
	return (void *)true;
}

//Prints Given Puzzle
void display_sudoku_puzzle(int sudoku_puzzle[GRID_SIZE][GRID_SIZE]){

    printf("CS49 Sudoku from Ricky Reyes\n\n");

	for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            	printf("%i ", sudoku_puzzle[i][j]);
        }
        printf("\n");
    }
}

// Is The Sudoku Puzzle Solved?
bool isSolved(bool solved, void *row, void *column, void *square[GRID_SIZE]){
		for ( int i=0; i < GRID_SIZE; i++){
			if ((bool)row == true && 
				(bool)column == true &&
				(bool)square[i] == true){
				solved = true;
			}
		}
		return solved;
}