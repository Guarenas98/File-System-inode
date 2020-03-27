#include "libfs.h"
#include "libdisk.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>


#define ARCHIVOS_POR_DIRECTORIO 30
#define iN_DIRECTORIO_DESOCUPADO -1
#define TYPE_ARCHIVO 1
#define TYPE_DIRECTORIO 2
#define BYTES_MAP_B 1250  // en 1250 bytes hay 10 000 bits, los cuales son para mapear los sectores/bloques del disco
#define BYTES_MAP_iN 125// en 125 bytes hay 1000 bits, los cuales serviran para mapear los 1000 inodos existentes
#define MAX_FILES  1000

char superbloque[] = "512 10000";


//de donde a donde se guarda cada estructura del Sistema de archivos          
//mapa de bits de bloques						    
int li_map_b;
int ls_map_b;        
int long_mapa;

//mapa de bits d inodos
int li_map_iN;
int ls_map_iN;

//sectores ocupados por inodos
int li_tabla_iN;
int ls_tabla_iN;

//punto (sector) de montaje del dir raiz
int montaje_dir_raiz;
int ls_dir_raiz;

//por cual sector inician los bloques de datos
int li_bloques_de_datos;


//estructuras de datos del sistema de archivos

typedef struct {
	int tamanio_archivo;  // tamanio en BLOQUES
	int type;  // directorio o archivo
	int sect_bloques[30];                   //guarda 30 numeritos, cada numerito sirve para ubicar el sector del bloque de datos(con disk_read)
} inodo_t;


typedef struct {
	char nombres[30][16];  //30 archivos con nombres de no mas de 16 caracteres	
	int* ptr_inodos;  //apuntara al dir_inodos[30]
	char nombre_dir;  // raiz "/"
} inodo_directorio_t;

int dir_inodos[30];  //guarda 30 numeritos, cada numerito se usa para ubicar struc de inodo en la tabla_inodo


//-------------------------------------------------------------------------------------------------------------------------
/*TABLA DE ARCHIVOS ABIERTOS*/
typedef struct{
	int fd;
	int offset;  //puntero de arhcivo, para escribir y leer desde posiciones
	bool ocupado;
	char *nombre_archivo; //de maximo 16 bits
	inodo_t *inodo_de_archivo;
} entrada_tabla;

entrada_tabla Tabla_De_Archivos_Abiertos[20] ; //La Tabla tiene 20 entradas, entonces es un arreglo de 20 entradas y yasta.
//--------------------------------------------------------------------------------------------------------------------------




//variables globales
int os_errno = -1; /* global errno value here */
char* camino_disco = NULL;
int cont_archivos_abiertos = 0;  //para limitar la apertura de archivos a 20
unsigned char bit_map_bloques[BYTES_MAP_B];  // contendra el mapping a partir del sect 1 del disco
unsigned char bit_map_inodos[BYTES_MAP_iN];  // contendra mapping de inodos
inodo_t tabla_inodos[MAX_FILES];  //contendra los mil inodos del SA
inodo_directorio_t dir_raiz;

/*IDEA DEL SISTEMA DE ARCHIVO:
1000 inodos para 1000 archivos

DISTRIBUCION DE LAS ESTRUCTURAS DEL SA EN EL DISCO:
Sector 0: Guarda el superbloque

sector 1: mapa de bits de inodos

sector [2-4]: mapa de bits de bloques de datos 

sector [5 -- 254]: para los inodes(bloques de inodes), los Bloque de inodes
estan conformados por 4 inodos compactados para caber en sectores de 512 bytes
son 250 bloques de inodes en total

sector[255 - 256]: para guardar el directorio raiz
NOTA: Directorio raiz se tuvo que dividir en 2 partes(dir0 y dir1) pues no cabe completo en 1 sector:
dir1 contendra
nombres de ficheros (480 bytes) y un apuntador(4 bytes) a al arreglo de inodes[30] => dir1 consume un poco menos de 500 bytes
dir2 contendra
el arreglo de inodes[30] (120 bytes)


sector [257 - 9 999]: el resto de los sectores del disco
seran bloques de datos que almacenan el contenido de los archivos


Descripcion Grafica del SA en el Disco:

  li_map_iN = ls_map_iN
            |             li_map_b                      ls_map_b            li_tabla_iN        ls_tabla_iN  pto_montaje    ls_montaje    li_bloques_de_datos
            | 		   |                                 |                |                  |		 |           |           |
            v              v                                 v                v                  v		 v           v           v
0           1              2                3                4                5                  254            255          256        257
[   SB    ] [ map_iNodos ] [ map_Bloques1 ] [ map_Bloques2 ] [ map_Bloques3 ] [ bloqu_iNodo1 ] ...[bloqu_iNodon] [   dir0  ] [  dir1  ] [bloque_Data] ......

recordar que los sectores de disco se acceden con la api del libdisk
Nota: en bitmapping, 1 indica libre, 0 ocupado

*/

int superbloque_es_valido(char* superb) {
	for(int i=0; i< strlen(superbloque); ++i) {
		if(superb[i] != superbloque[i]) {
			return 0;
		}
	}
	return 1;
}


/*api de bit_map*/
void clear_bit_map(unsigned char* bit_array, int lim_bytes) {  //pone todos los bits en 0
	for(int i=0; i < lim_bytes; ++i){
		bit_array[i] = 0;
	}
}


void set_bit_map(unsigned char* bit_array, int lim_bytes) {  //pone todos los bits en 1
	for(int i=0; i < lim_bytes; ++i){
		bit_array[i] = 255;
	}
}


unsigned char read_bit(unsigned char* bit_array, int bit) { //lee el bit pero lo retorna en el primer bit de value
	unsigned char value = (bit_array[bit / 8] & (1 << (bit % 8)) ) != 0;
	return value;

}

void set_bit(unsigned char* bit_array, int bit) {  //coloca el bit en 1
	bit_array[bit / 8] |= (1 << (bit % 8));
}

void clear_bit(unsigned char* bit_array, int bit) {  //coloca el bit en 0
	bit_array[bit / 8] &= ~(1 << (bit % 8));
}


void test_estructuras() {  //muestra el estado de las variables y estructuras del proyecto
	int opcion; //-1 prueba de limites 0 consultar bitmap_bloq, 1 consultar bitmap_iN, 2 consultar tabla inodos,  3consultar archivos del directorio  //
	//char *entrada=NULL;
	char entrada[16];
	int posicion;
	char* buffer_rw = NULL;
	int numerito;
	size_t n=16;
	while(1) {
	puts("-1) Salir\n 0)Ver BITMAP bloques\n 1)BITMAP inodos\n 2) estado inodos(hasta 30)\n 3)estado Directorio /\n 4) PesoEstructuras\n 5) File_create\n6) File_unlink");
	puts("7) Tabla de Arh.Abiertos\n 8) file_open\n 9) File_close\n 10) File_seek\n 11) file_write\n 12) file_read\n 13) fs_sync");
	scanf("%i", &opcion);
	switch(opcion) {			
		case -1:  //testeo del mapbit
			return;
		break;
		case 0:
			puts("bitmap bloques");
			for(int i=0; i < 10000; ++i) {
				printf("%i ", read_bit(bit_map_bloques, i));
			}
			printf("\n");
		break;	
		case 1:
			puts("bitmap iNodos");
			for(int i=0; i < 1000; ++i) {
				printf("%i ", read_bit(bit_map_inodos, i));
			}
			printf("\n");
		break;
		case 2:
			for(int i=0; i < 30; ++i) {
				printf("inodo %i:\n", i);
				printf("	tamanio: %i\n", tabla_inodos[i].tamanio_archivo);
				for(int j=0; j < 30; j++) {
					printf("	bloque %i: %i\n", j, tabla_inodos[i].sect_bloques[j]);
				}

			}
			printf("\n");
		break;
		case 3:
			printf("directorio raiz: %c\n", dir_raiz.nombre_dir);
				for(int i=0; i < 30; i++) {
					printf("Nombre archivo: ");
					for (int j = 0; j < 16; j++){
						printf("%c", dir_raiz.nombres[i][j]);
					}
					printf("\n");
					
					
					printf("dir_inodos[%i]: %i \n\n", i ,dir_inodos[i]);
				}
				printf("\n");
		break;
		case 4:
			printf("tamanios en bytes\n");
			printf("	inodo_t: %i\n", sizeof(inodo_t));
			printf("	inodo_directorio_t: %i\n", sizeof(inodo_directorio_t));
			printf("	bitmap inodos: %i\n", sizeof(bit_map_inodos));
			printf("	bitmap bloques: %i\n", sizeof(bit_map_bloques));
			printf("	tabla inodos: %i\n", sizeof(tabla_inodos));
			printf("\n");
		break;
		case 5:    //testeo de file_create
			puts("Dime el nombre del nuevo archivo: ");
			//getline(&entrada,&n,stdin);
			//getline(&entrada,&n,stdin);	
			scanf("%s", entrada);
			printf("file_create retorno %i\n\n",file_create(entrada));

		break;
		case 6:  //testeo file_unlink
			puts("nombre de archivo a eliminar");
			//getline(&entrada,&n,stdin);
			//getline(&entrada,&n,stdin);
			scanf("%s", entrada);
			printf("file_unlink retorno %i \n", file_unlink(entrada));
		break;
 		
		case 7:
			puts("estado tabla de abiertos:");
			for(int i=0; i < 20; i++) {
				printf("ocupado: %i. nombre: %s. fd: %i. offset: %i\n", Tabla_De_Archivos_Abiertos[i].ocupado ,(char*)Tabla_De_Archivos_Abiertos[i].nombre_archivo, Tabla_De_Archivos_Abiertos[i].fd, Tabla_De_Archivos_Abiertos[i].offset);

			}
			printf("archivos abiertos: %i\n", cont_archivos_abiertos);
		break;
		case 8:   //test file_open
			puts("archivo a abrir");
	//		getline(&entrada,&n,stdin);
	//		getline(&entrada,&n,stdin);
			scanf("%s", entrada);
			printf("open retorno: %i\n", file_open(entrada));
		break;
		case 9:  //test file_close
			puts("decime el descriptor de archivo a cerrar wey:");
			scanf("%i", &numerito);
			file_close(numerito);
		break;
		case 10:  //fseek
			puts("dime descriptor:");
			scanf("%i", &numerito);
			puts("dime desplazamiento:");
			scanf("%i", &posicion);
			file_seek(numerito, posicion);
			puts("offset movido.\n");
			
		break;
		case 11:  //fwrite saul
			puts("Dime descriptor:");
			scanf("%i", &numerito);
			puts("cuantos bytes quieres escribir?");
			
			scanf("%i", &n);
			buffer_rw = malloc(n);
			puts("que quiere escribir?");
			scanf("%s", buffer_rw);
			n = file_write(numerito, buffer_rw, n);
			printf("file_write escribio %i bytes\n", n);
			free(buffer_rw);
		break;
		case 12:  //fread saul
			puts("Dime descriptor:");
			scanf("%i", &numerito);
			puts("cuantos bytes quieres leer?");
			
			scanf("%i", &n);
			buffer_rw = malloc(n);
			file_read(numerito, buffer_rw, n);
			printf("file_read leyo: %s\n", buffer_rw);
			free(buffer_rw);
		break;
		case 13:
			printf("fs_sync retorno: %i\n", fs_sync());
	}
	printf("\nestado os_errno:%i\n\n", os_errno);
	}

}



int fs_boot(char *path) {  //inicializa las estructuras del SA(en memoria)
	os_errno = -1;
	int i = 0;
	int j = 0;
	camino_disco = path;
	if (disk_init() == -1) {   							//DISK_INIT() Listo
		//fprintf(stderr, "fs_boot() failed\n");
		os_errno = E_GENERAL;
		return -1;
	}

	//mapa de bits d inodos
	li_map_iN = 1;
	ls_map_iN = li_map_iN;

	//mapa bits d bloques
	li_map_b = ls_map_iN + 1;
	ls_map_b = ((4096 * 8)/ 10000) + li_map_b - 1;                //sect 4       
	long_mapa = 10000;  //longitud de bits del mapa        

	//sectores ocupados por tabla inodos
	li_tabla_iN = ls_map_b + 1;                                   //sect 5
	ls_tabla_iN = (MAX_FILES / 4) + li_tabla_iN - 1;              //sect 254

	//sector ocupado por inodo raiz
	montaje_dir_raiz = ls_tabla_iN + 1;		
	ls_dir_raiz = montaje_dir_raiz + 1;	

	li_bloques_de_datos = ls_dir_raiz + 1;                        //sect 257


	//determinar archivo de Disco
	disk_load(path);
	if(disk_errno == E_OPENING_FILE) {  //caso 1: fichero no existe		
		
		/*inicializando valores de arreglos relevantes en 0 para indicar q sus posiciones estan disponibles*/
		/*======================================================================*/
		for ( i = 0; i < 1000; i++){
			for ( j = 0; j < 30; j++){
				tabla_inodos[i].sect_bloques[j] = 0;
			}
		}
		/*======================================================================*/

		/*inicializa directorio*/
		dir_raiz.nombre_dir = '/';
		for(i=0; i < ARCHIVOS_POR_DIRECTORIO; i++) {
			dir_inodos[i] = iN_DIRECTORIO_DESOCUPADO;
		}
		//formatearlo:insertar superblock, bitmaps, inodes..
		if(disk_write(0, superbloque) == -1) {
			os_errno = E_GENERAL;
			return -1;
		}

		//1)setear mapa de inodos
		set_bit_map(bit_map_inodos, BYTES_MAP_iN);
		//2) setear mapa de bloques
		set_bit_map(bit_map_bloques, BYTES_MAP_B);
		//3) limpiar bits de mapa que indican que las estrcuturas del SA ocupa las primeras posiciones
		for(int i = 0; i < li_bloques_de_datos; ++i) {  //marcar los primeros sectores como ocupados(por las estructuras del SA)
			clear_bit(bit_map_bloques, i);
		}

		fs_sync();  //crea el archivo que no existe, y graba formateo en este

	} else {  //caso 2:fichero existe
		char* buffer = (char*)malloc(sizeof(Sector));
		disk_read(0, buffer);
		FILE* f = fopen(path, "r");
		fseek(f, 0, SEEK_END);
		size_t tam_archivo = ftell(f);
		if(superbloque_es_valido(buffer) == 0 || tam_archivo != SECTOR_SIZE * NUM_SECTORS) {  //acciones pertinentes para comprobar que el disco sea valido(PDF)
			//fprintf(stderr, "fs_boot() failed\n");
			os_errno = E_GENERAL;
			return -1;
		}
		fclose(f);
		free(buffer);
		
		//extraer las estructuras del sistema de archivos para manipularlos
		//1) extraer map inodos		
		if(disk_read(li_map_iN, (char*)bit_map_inodos) == -1) {
			//fprintf(stderr, "fs_boot() failed\n");
			os_errno = E_GENERAL;
			return -1;
		}

		//2) extraer mapa de bloques
		for(int sector = li_map_b; sector <= ls_map_b; sector++) {
			if(disk_read(sector, (char*)&bit_map_bloques[ SECTOR_SIZE * (sector - li_map_b) ]) == -1) {
				//fprintf(stderr, "fs_boot() failed\n");
				os_errno = E_GENERAL;
				return -1;
			}
		}
		//3) extraer inodos de 4 en 4
		for(int sector = li_tabla_iN ; sector <= ls_tabla_iN; ++sector) {	
			if(disk_read(sector, (char*)&tabla_inodos[ (sector-li_tabla_iN) * 4 ]) == -1) {
				//fprintf(stderr, "fs_boot() failed\n");
				os_errno = E_GENERAL;
				return -1;		
			}
		}

		//obtener dir_raiz del punto de montaje(dir0)
		if(disk_read(montaje_dir_raiz, (char*)&dir_raiz) == -1) {
			//fprintf(stderr, "fs_boot() failed\n");
			os_errno = E_GENERAL;
			return -1;
		}
		//obtener dir_inodos(dir1)
		if(disk_read(ls_dir_raiz, (char*)dir_inodos) == -1) {
			//fprintf(stderr, "fs_boot() failed\n");
			os_errno = E_GENERAL;
			return -1;
		}
	}

	//enlace de dir0 con dir1
	dir_raiz.ptr_inodos = dir_inodos;
	

	/* ininializar posiciones libres de tabla de archivos abierto*/	
	for(i = 0; i < 20; i++) {
		Tabla_De_Archivos_Abiertos[i].ocupado = false;
		Tabla_De_Archivos_Abiertos[i].nombre_archivo = NULL;
		Tabla_De_Archivos_Abiertos[i].fd = i;
		Tabla_De_Archivos_Abiertos[i].offset = 0;
	}

	return 0;
}

int fs_sync(void) {		
	//actualizar estado de las estructuras en el buffer disk
	//mapa de inodos
	if(disk_write(li_map_iN, (char*)bit_map_inodos) == -1) {	
		//fprintf(stderr, "fs_sync failed\n");
		os_errno = E_GENERAL;
		return -1;
	}
	//mapa de bloques
	for(int sector = li_map_b;  sector <= ls_map_b; ++sector) {  //recorrer sectores para meter el mapa parte por parte(512 bytes a 512 bytes)
		if( disk_write(sector, (char*)&bit_map_bloques[SECTOR_SIZE * (sector - li_map_b)]) == -1 ) {
			//fprintf(stderr, "fs_sync failed\n");
			os_errno = E_GENERAL;
			return -1;
		}
	}
	//conjunto de inodos
	for(int sector=li_tabla_iN; sector <= ls_tabla_iN; ++sector){			//FORMULA: ubicar un inodo j en un sector i: (sector - li_sectores asignados a inodos)*4 + j
		if( disk_write(sector, (char*)&tabla_inodos[ (sector - li_tabla_iN) * 4 ]) == -1) {
			//fprintf(stderr, "fs_sync failed\n");
			os_errno = E_GENERAL;
			return -1;
		}
			
	}
	//dir_raiz.ptr_inodos = NULL;
	if(disk_write(montaje_dir_raiz, (char*) &dir_raiz) == -1)   //inodo del directorio raiz(dir0)
 	{
		//fprintf(stderr, "fs_sync failed\n");
		os_errno = E_GENERAL;
		return -1;
	}
	if(disk_write(ls_dir_raiz, (char*)dir_inodos) == -1)   // ubicacion inodos de archivos (dir1)
 	{
		//fprintf(stderr, "fs_sync failed\n");
		os_errno = E_GENERAL;
		return -1;
	}

	//actualizar el buffer disk en el archivo
	if(disk_save(camino_disco) == -1) {
		//fprintf(stderr, "fs_sync failed\n");
		os_errno = E_GENERAL;
		return -1;
	}
	return 0;
}


int file_create(char *file)
{	
	//los bloques de datos van desde el sector 255 i_bloques_de_datos=255
	//unsigned char bloque_actual;
	//tomar el inodo i de la tabla de inodos(tabla_inodo[i])
	int cont=0, ino, i,v;
	int blo;

	if(file == NULL || strlen(file) > 15) {
		os_errno = E_CREATE;
		return -1;
	}

	/*SI YA EXISTE=================================*/
	for(v=0;v<ARCHIVOS_POR_DIRECTORIO;v++){
		if ( strncmp(dir_raiz.nombres[v], file, 16) == 0) //si ya existe el archivo (si hay uno con el mismo nombre)
		{
			os_errno=E_CREATE;
			//fprintf(stderr, "fs_create() failed: el archivo ya existe\n");
			return -1;
		}
		
	}
	
	/*SI NO HAY SUFICIENTE ESPACIO*/	
	/*===============================================*/

	/*HAY QUE ASIGNAR EL INODO Y AGREGAR EL NOMBRE*/
			
		//MODIFICANDO INODO DIRECTORIO
		for ( i = 0; i < ARCHIVOS_POR_DIRECTORIO; i++) {
			/*SOLO DEBE ENTRAR UNA VEZ*/
			/*===============================================================*/


			/*Agregando nombre e inodo*/
			if( dir_raiz.ptr_inodos[i] == iN_DIRECTORIO_DESOCUPADO ){//si está en NULL o libre			
				strncpy(&dir_raiz.nombres[i][0],file,16);
				//Ocupar Inodo
				for ( ino = 0; ino < MAX_FILES; ino++){
					if(read_bit(bit_map_inodos,ino) == 1){
						clear_bit(bit_map_inodos,ino); //OCUPO EL INODO
						//y lo empiezo a editar
			
						/*AQUI LE DAMOS EL INODO AL DIRECTORIO*/
						/*=================*/
						dir_inodos[i] = ino;
						/*===================*/
		
						tabla_inodos[ino].tamanio_archivo = 0; //tamaño inicial 0 
						tabla_inodos[ino].type = TYPE_ARCHIVO;									
						
						break;
						/*ino ahora tendrá el valor de indice del inodo*/
					}
					/*============================================*/
					if(ino==MAX_FILES-1){ //es que esta full
						os_errno = E_NO_SPACE;
						//fprintf(stderr, "fs_create() failed\n");
						return-1;
					}
					/*============================================*/					
				}		
				break;
			}
			/*===============================================================*/

		} //for de recorrido directorio
					
		/*=========================*/
		if(i==30){ //es que esta full, los 30 inodos ocupados
			os_errno = E_NO_SPACE;
			//fprintf(stderr, "fs_create() failed: directorio lleno (30 archivos maximo)...\n");
			return-1;
		}
		/*==========================*/

		//asignar bloques: 	/*SI YA SE HAN ASIGNADO 30 BLOQUES SE SALE*/
		cont = 0;
		for ( blo = li_bloques_de_datos; blo < NUM_SECTORS && cont < 30; blo++)
		{
			if(read_bit(bit_map_bloques,blo) == 1){ //si el bloque está disponible/libre
				clear_bit(bit_map_bloques,blo);//OCUPO EL BLOQUE DE DATOS
				/*ASIGNO EL BLOQUE DE DATOS*/
				tabla_inodos[ino].sect_bloques[cont] = blo;
				cont++;
			}
		}				
		
		/*SI HAY BLOQUES DISPONIBLES PERO NO LLEGAN A 30*/
		if(blo==NUM_SECTORS-1 && cont<30 && cont>=1) {
			return 0; 
		}
	
	os_errno=E_CREATE;
	//fprintf(stderr, "fs_create(): archivo creado\n");
	return 0;
}



int file_open(char *file)    //file_open listo
{
	//comprobar que fichero no existe en el SA
	int archivo;
	bool file_existe = false;
	for(archivo=0; archivo < ARCHIVOS_POR_DIRECTORIO; ++archivo) {  //recorrer dir_raiz.nombres[];
		if( strncmp( dir_raiz.nombres[archivo], file, 16)  ==  0) {
			file_existe = true;
			break;
		}
	}
	if(file_existe == false) {
		os_errno = E_NO_SUCH_FILE;
		//fprintf(stderr, "fs_open failed()\n");
		return -1;
	}

	//comprobar limite de archivos abiertos
	if(cont_archivos_abiertos >= 20) {
		os_errno = E_TOO_MANY_OPEN_FILES;
		//fprintf(stderr, "fs_open failed()\n");
		return -1;
	}
	
	//hacer asignacion a tabla de abiertos
	int pos_libre=0;
	for(int i=0; i < 20; i++) {  //buscar entrada libre en la tabla de abiertos
		if(Tabla_De_Archivos_Abiertos[i].ocupado == false) {
			pos_libre = i;
			break;
		}
	}
	Tabla_De_Archivos_Abiertos[pos_libre].nombre_archivo = (char*)&dir_raiz.nombres[archivo];
	Tabla_De_Archivos_Abiertos[pos_libre].inodo_de_archivo = &tabla_inodos[ dir_raiz.ptr_inodos[archivo] ];
	Tabla_De_Archivos_Abiertos[pos_libre].ocupado = true;
	Tabla_De_Archivos_Abiertos[pos_libre].offset = 0;
	cont_archivos_abiertos++;	
	
	return (Tabla_De_Archivos_Abiertos[pos_libre].fd);
}



int file_read(int fd, void *buffer, int size)  //saul
{
	int i, posicionado;
	realloc(buffer, size);
	char *bufalo=calloc(size,sizeof(char));

	if(Tabla_De_Archivos_Abiertos[fd].ocupado == false) {
		os_errno = E_BAD_FD;
		free(bufalo);
		return -1;	
	}
	/*SI EL ARCHIVO NO ESTA ABIERTO*/
	if(!(fd<30 && fd>=0)){
		os_errno = E_BAD_FD;
		free(bufalo);
		return -1;
	}
	if(Tabla_De_Archivos_Abiertos[fd].ocupado==false){
		os_errno = E_BAD_FD;
		free(bufalo);
		return -1;
	}

	for(i=0 ; i<size ;i++){

		posicionado = Tabla_De_Archivos_Abiertos[fd].offset/512;

		disk_read(  Tabla_De_Archivos_Abiertos[fd].inodo_de_archivo->sect_bloques[posicionado] ,  bufalo);

		((char *)buffer)[i] = bufalo[(Tabla_De_Archivos_Abiertos[fd].offset) % 512];

		/*========================================================================*/
		/*SI EL OFFSET ESTÁ AL FINAL DE LARCHIVO*/
		if(Tabla_De_Archivos_Abiertos[fd].offset == 511 && posicionado == 29){
			return 0;
		}
		/*========================================================================*/

		Tabla_De_Archivos_Abiertos[fd].offset++;
	}

	free(bufalo);
	return 0;
}


int file_write(int fd, void *buffer, int size)
{
	/*bytes de 0 a 511 por bloque*/
	int i,    posicionado=0;
	char *bu512fer = calloc(512,sizeof(char));
	
	if(!(fd<30 && fd>=0)){
		os_errno = E_BAD_FD;
		free(bu512fer);
		return -1;
	}
	
	if(Tabla_De_Archivos_Abiertos[fd].ocupado==true){ //el archivo se encuentra abierto
		
		for(i=0;i<strlen(buffer);i++){
			
			posicionado = Tabla_De_Archivos_Abiertos[fd].offset/512;//me paro en el bloque del ofsett automaticamente con esa div

			/*SI HAY QUE HACER MAS ESPACIO*/
			/*===============================================================*/
			/*SI EL DISCO DURO ESTÁ LLENO*/
			if(Tabla_De_Archivos_Abiertos[fd].inodo_de_archivo->sect_bloques[posicionado]==0) {//como tenemos asignacion contigua y de 30, que una posicion de sect_bloques esté en 0 quiere decir que llegamos al final del disco así que no hay espacio suficiente
				os_errno = E_NO_SPACE;
				free(bu512fer);
				return -1;
			}
			/*===============================================================*/
			/*AUMENTANDO EL ESPACIO*/
			if( Tabla_De_Archivos_Abiertos[fd].inodo_de_archivo->tamanio_archivo == posicionado && Tabla_De_Archivos_Abiertos[fd].inodo_de_archivo->tamanio_archivo<30){
				Tabla_De_Archivos_Abiertos[fd].inodo_de_archivo->tamanio_archivo++;
			}
			/*===============================================================*/
			//SI EXCEDE EL TAMAÑO MAXIMO PERMITIDO 30 (DE 0 A 29)
			else if(Tabla_De_Archivos_Abiertos[fd].inodo_de_archivo->tamanio_archivo == posicionado && Tabla_De_Archivos_Abiertos[fd].inodo_de_archivo->tamanio_archivo>=30 ){
				os_errno = E_FILE_TOO_BIG;
				free(bu512fer);
				return -1;
			}
			/*===============================================================*/

			disk_read(  Tabla_De_Archivos_Abiertos[fd].inodo_de_archivo->sect_bloques[posicionado]  ,   bu512fer );
			
			bu512fer[(Tabla_De_Archivos_Abiertos[fd].offset) % 512] = ((char *)buffer)[i]; //aquí el mod para obtener el byte,
			//printf("poscicion que escribe: %d | offset: %d\n", ((Tabla_De_Archivos_Abiertos[fd].offset) % 512),Tabla_De_Archivos_Abiertos[fd].offset);
			disk_write(  Tabla_De_Archivos_Abiertos[fd].inodo_de_archivo->sect_bloques[posicionado]  , bu512fer);

			Tabla_De_Archivos_Abiertos[fd].offset++;	
		}
	
	}else{ //si el archivo no está abierto
		os_errno = E_BAD_FD;
		free(bu512fer);
		return -1;
	}

	fs_sync();
	free(bu512fer);
	//fprintf(stderr, "fs_write correcto\n");
	return size;	
}



int file_seek(int fd, int offset)
{
	if(!(fd<30 && fd>=0)){
		os_errno = E_BAD_FD;
		return -1;
	}

	if(Tabla_De_Archivos_Abiertos[fd].ocupado==false){
		os_errno = E_BAD_FD;
		return -1;
	}

	if (offset >= 0 && offset < 15360) //si estén el rango correcto
	{
		Tabla_De_Archivos_Abiertos[fd].offset = offset;
		return offset;
	}else{
		os_errno=E_SEEK_OUT_OF_BOUNDS;
		return -1;
	}
	
	return -1;
	//fprintf(stderr, "fs_seek\n");

}


int file_close(int fd)
{
	if(fd < 0 || fd > 20) {
		os_errno = E_BAD_FD;
		return -1;
	}
	if(Tabla_De_Archivos_Abiertos[fd].ocupado) {
		Tabla_De_Archivos_Abiertos[fd].ocupado = false;
		Tabla_De_Archivos_Abiertos[fd].nombre_archivo = NULL;
		Tabla_De_Archivos_Abiertos[fd].inodo_de_archivo = NULL;
		Tabla_De_Archivos_Abiertos[fd].offset = 0;
		cont_archivos_abiertos--;
	}
	//fprintf(stderr, "fs_close\n");
	return 0;
}

int file_unlink(char *file)   //file_unlink funcional, puede ser probado
{
	//acceder al inodo del directorio, y buscar el nombre especificado
	int archivo;
	bool file_esta_abierto = false;
	bool file_existe = false;
	for(archivo=0; archivo < ARCHIVOS_POR_DIRECTORIO; ++archivo) {  //recorrer dir_raiz.nombres[];
		if( strncmp( dir_raiz.nombres[archivo], file, 16)  ==  0) {
			file_existe = true;
			break;
		}
	}
	
	//recorrer tabla de abietos a ver si esta en uso el archivo
	for(int item_tabla=0; item_tabla < 20; item_tabla++) {
		if(Tabla_De_Archivos_Abiertos[item_tabla].nombre_archivo != NULL && strncmp(Tabla_De_Archivos_Abiertos[item_tabla].nombre_archivo, file, 16) == 0) {
			file_esta_abierto = true;
			break;
		}
	}


	if(file_existe == false) {	//caso archivo no existe
		//fprintf(stderr, "file_unlink() failed\n");
		os_errno = E_NO_SUCH_FILE;
		return -1;
	}

	if(file_esta_abierto == true) {   //caso archivo en tabla de abiertos
		//fprintf(stderr, "file_unlink() failed\n");
		os_errno = E_FILE_IN_USE;
		return -1;
	} else {  // caso archivo puede eliminarse

		//acceder inodo y liberar los 30 bloques reservados para el archivo, y limpiar atributos de inodo
		int ubicacion_inodo = dir_raiz.ptr_inodos[archivo];
		tabla_inodos[ubicacion_inodo].tamanio_archivo = 0;
		inodo_t* nodo_a_liberar = &tabla_inodos[ubicacion_inodo];
		for(int bloque= 0; bloque < 30; bloque++) {
			int ubicacion_bloque = nodo_a_liberar -> sect_bloques[bloque];
			disk_write(ubicacion_bloque, " ");  // borrar toda la info que haya en el sector de 512 bytes del disco duro
			tabla_inodos[ubicacion_inodo].sect_bloques[bloque] = 0;  //se le quita numeritos de sector/bloque
			set_bit(bit_map_bloques, ubicacion_bloque);		//actualizar mapa de bits de bloques
		}
		//limpiar campos (nombre_archivo e dir_inodo) del diectorio raiz, para indicar que ya no existe el archivo		
		memset(dir_raiz.nombres[archivo] , 0 ,16);	
		dir_raiz.ptr_inodos[archivo] = iN_DIRECTORIO_DESOCUPADO;
		set_bit(bit_map_inodos, ubicacion_inodo);
	}
	return 0;
}