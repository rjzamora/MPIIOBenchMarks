#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

#define MB            1048576
#define SEED          12173
#define RANGE         100
#define MAX_CHAR_PATH 4096

double PFS_Uncache (char *filename)
{
	int rank, i, err;
	char tmp_file[MAX_CHAR_PATH];
	char tmp_path[MAX_CHAR_PATH];
	double start_time, end_time;
	MPI_File tmp_fh;
	MPI_Status status;
	int64_t el = 2500000;
	float *val, *val_r;

	start_time = MPI_Wtime ();
	strcpy (tmp_path, filename);

	/* 10 MB of float values per array */
	val   = (float *) malloc ( el * sizeof(float));
	val_r = (float *) malloc ( el * sizeof(float));

	MPI_Comm_rank (MPI_COMM_WORLD, &rank);
	snprintf (tmp_file, MAX_CHAR_PATH, "%s/uncache-%09d.tmp", tmp_path, rank);

	for (i = el; i > 0; i--) val[i-1] = (float)i;

	MPI_File_open(MPI_COMM_SELF, tmp_file, MPI_MODE_WRONLY | MPI_MODE_CREATE, MPI_INFO_NULL, &tmp_fh);
	MPI_File_write (tmp_fh, val,   el, MPI_FLOAT, &status);
	MPI_File_write (tmp_fh, val_r, el, MPI_FLOAT, &status);
	MPI_File_close (&tmp_fh);
	MPI_File_delete (tmp_file, MPI_INFO_NULL);

	free (val);
	free (val_r);
	MPI_Barrier (MPI_COMM_WORLD);

	end_time = PMPI_Wtime ();
	return end_time - start_time;
}


int main(int argc, char * argv[])
{

	/* Variable definitions */
	char * filename = "file.mpio";  // name of file to write
	char * funcache = "dummy.mpio"; // name of file to write for "unchaching"
	char * outname  = "result.txt"; // name of file to record timing numbers
	int * lrandarr;                 // pointer to data buffer to write
	int * lrandarr_r;               // pointer to data buffer to read/check
	int * offsets;                  // MPI-dataype offsets array
	long range, i;                  // variable integers (long)
	int nel, block, count, uncache; // variable integers (int)
	double tC, tR, tW, tS, tUC;     // timers
	int returnval = 0;              // return value
	FILE *fp;                       // Output file for timing

	/* MPI variables */
	int rank, size;
	MPI_Info info = MPI_INFO_NULL;
	MPI_Comm comm = MPI_COMM_WORLD;
	MPI_File fhr, fhw;
	MPI_Status status;
	MPI_Offset offset0;
	MPI_Datatype indexedtype;

	/* Initialize MPI */
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	/* Command-line args */
	block = 1024;
	count = 1;
	uncache = 1;
	for (i=1;i<argc;i++) {
		if (strcmp(argv[i], "--file") == 0){
			i++; filename = argv[i];
		} else if (strcmp(argv[i], "--out") == 0){
			i++; outname = argv[i];
		} else if (strcmp(argv[i], "--block") == 0){
			// Size of "chunks" written by each rank
			i++; block = atoi( argv[i] );
		} else if (strcmp(argv[i], "--count") == 0){
			// Number of "chunks" written by each rank
			i++; count = atoi( argv[i] );
		} else if (strcmp(argv[i], "--cache") == 0){
			// Allow Possible Caching
			uncache = 0;
		}
	}
	nel = block * count;
	if(rank==0) fp=fopen(outname, "a");
	MPI_Barrier(MPI_COMM_WORLD);

#if 0
	/* Optional: Define MPI 'info' object */
	MPI_Info_create( &info );
	MPI_Info_set( info, "striping_unit", "4194304");
	MPI_Info_set( info, "striping_factor", "-1");
#endif

	/* Initialize arrays */
	lrandarr = (int *) malloc(nel * sizeof(int));
	lrandarr_r = (int *) malloc(nel * sizeof(int));
	srand(SEED+rank*SEED);
	for(i=0; i<nel; i++){
		lrandarr[i] = rand() % RANGE;
		lrandarr_r[i] = 0;
	}
	offset0 = block * rank * sizeof(int); // 0th offset for this rank
	if (count > 1) {
		offsets = (int*) malloc(count * sizeof(int));
	}


	/* Start the timer */
	MPI_Barrier( MPI_COMM_WORLD );
	tC = MPI_Wtime();

	/* Let rank 0 Create the file */
	long int filesize = sizeof(int) * size * nel;
	if(rank == 0){
		MPI_File_open( MPI_COMM_SELF, filename, MPI_MODE_CREATE|MPI_MODE_WRONLY, info, &fhw );
		MPI_File_set_size( fhw, filesize );
		MPI_File_close( &fhw );
	}

	/* Check the timer */
	MPI_Barrier(MPI_COMM_WORLD);
	tC = MPI_Wtime() - tC;



	/* Open the file for writing */
	MPI_File_open( MPI_COMM_WORLD, filename, MPI_MODE_WRONLY, info, &fhw );
	MPI_File_set_atomicity( fhw, 0 );

	/* Start the timer */
	MPI_Barrier(MPI_COMM_WORLD);
	tW = MPI_Wtime();

	/* Use MPI datatype if write is discontiguous for each rank */
	if (count > 1) {

		/* Populate datatype-helper array */
		for (i=0;i<count;i++) {
			offsets[i] = i * (block * size) + (rank * block);
		}

		/* Create and commit the new datatype */
		MPI_Type_create_indexed_block(count, block, offsets, MPI_INT, &indexedtype);
		MPI_Type_commit(&indexedtype);

		/* Set file view using the new datatype */
		MPI_File_set_view(fhw, 0, MPI_INT, indexedtype, "native", info);

		/* Write the file */
		MPI_File_write_all( fhw, lrandarr, nel, MPI_INT, &status );

	} else {

		/* Write the file */
		MPI_File_write_at_all( fhw, offset0, lrandarr, nel, MPI_INT, &status );

	}

	/* Stop the timer */
	MPI_Barrier(MPI_COMM_WORLD);
	tS = MPI_Wtime();
	tW = tS - tW;

	/* Sync the File */
	MPI_File_sync(fhw);

	/* Stop the timer */
	MPI_Barrier(MPI_COMM_WORLD);
	tS = MPI_Wtime() - tS;

	/* Close the file */
	MPI_File_close( &fhw );



	/* Write a dummy file to inhibit caching effects */
	if (uncache)
		tUC = PFS_Uncache( funcache );



	/* Open the original file for reading */
	MPI_File_open( MPI_COMM_WORLD, filename, MPI_MODE_RDONLY, info, &fhr);
	MPI_File_set_atomicity( fhr, 0 );

	/* Start the timer */
	MPI_Barrier(MPI_COMM_WORLD);
	tR = MPI_Wtime();

	/* Use MPI datatype if write is discontiguous for each rank */
	if (count > 1) {

		/* Populate datatype-helper array */
		for (i=0;i<count;i++) {
			offsets[i] = i * (block * size) + (rank * block);
		}

		/* Create and commit the new datatype */
		MPI_Type_create_indexed_block(count, block, offsets, MPI_INT, &indexedtype);
		MPI_Type_commit(&indexedtype);

		/* Set file view using the new datatype */
		MPI_File_set_view(fhr, 0, MPI_INT, indexedtype, "native", info);

		/* Write the file */
		MPI_File_read_all( fhr, lrandarr_r, nel, MPI_INT, &status );

	} else {

		/* Write the file */
		MPI_File_read_at_all( fhr, offset0, lrandarr_r, nel, MPI_INT, &status );

	}

	/* Stop the timer */
	MPI_Barrier(MPI_COMM_WORLD);
	tR = MPI_Wtime() - tR;

	/* Close the file */
	MPI_File_close( &fhr );


	/* Write BDWTH */
	double MBs = ((double) (sizeof(int) * size * nel) / ((double) MB));
	double write_bdwth = MBs / tW;
	double write_bdwth_sync = MBs / (tW + tS);
	double read_bdwth = MBs / tR;
	if(rank==0) {
		fprintf(fp,"\n1D Integer-Array MPI-IO Benchmark (ranks=%d, block=%d, count=%d, sizeof(int)=%d):\n", size, block, count, sizeof(int));
		fprintf(fp,  "WRITE(RAW):  SIZE[MB]= %f BDWTH[MB/s]= %f TIME[s]= %f\n", MBs, write_bdwth, tW);
		fprintf(fp,  "WRITE(SYNC): SIZE[MB]= %f BDWTH[MB/s]= %f TIME[s]= %f\n", MBs, write_bdwth_sync, tW+tS);
		fprintf(fp,  "READ(RAW):   SIZE[MB]= %f BDWTH[MB/s]= %f TIME[s]= %f\n", MBs, read_bdwth, tR);
		fprintf(fp,  "UNCACHE:     TIME[s]= %f\n", tUC);
		fclose(fp);
	}
	MPI_Barrier( MPI_COMM_WORLD );

	/* Check that the file is "correct" */
	for(i=0; i<nel; i++){
		int diff = lrandarr_r[i] - lrandarr[i];
		if (diff != 0){
			printf(" Error on Rank %d - i=%ld, written=%d, read=%d\n", rank, i, lrandarr[i], lrandarr_r[i]);
			returnval = 1; break;
		}
	}

	/* Free arrays */
	free(lrandarr);
	free(lrandarr_r);

	/* Finalize MPI */
	MPI_Barrier( MPI_COMM_WORLD );
	MPI_Finalize();
	return (returnval);
}
