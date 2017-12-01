/*
 * File-related system call implementations.
 */

#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/limits.h>
#include <kern/seek.h>
#include <kern/stat.h>
#include <lib.h>
#include <uio.h>
#include <proc.h>
#include <current.h>
#include <synch.h>
#include <copyinout.h>
#include <vfs.h>
#include <vnode.h>
#include <openfile.h>
#include <filetable.h>
#include <syscall.h>


/*
 * open() - get the path with copyinstr, then use openfile_open and
 * filetable_place to do the real work.
 */
int
sys_open(const_userptr_t upath, int flags, mode_t mode, int *retval)
{
	const int allflags = O_ACCMODE | O_CREAT | O_EXCL | O_TRUNC | O_APPEND | O_NOCTTY;
	char *kpath;
	struct openfile *file;
	int result = 0;
	/*
	 * Your implementation of system call open starts here.
	 *
	 * Check the design document design/filesyscall.txt for the steps
         *
	 */
	kpath = kmalloc(PATH_MAX);

	//copying user path
	copyinstr(upath,kpath,PATH_MAX, NULL);
	if((flags & allflags) == flags){
		result = openfile_open(kpath, flags, mode, &file);
	}else
		return -1;	//INVALID FLAG

	if(result){
		return result;
	}
	//adding it to the filetable
	filetable_place(curproc->p_filetable,file,retval);

	return 0;
}

/*
 * read() - read data from a file
 */
int
sys_read(int fd, userptr_t buf, size_t size)
{
       int result = 0;
       struct openfile *file;
       struct uio myuio;
       struct iovec myiov;

       /*
        * Your implementation of system call read starts here.
        *
        * Check the design document design/filesyscall.txt for the steps
        */


	//grabbing the openfile from the filetable
	filetable_get(curproc->p_filetable, fd, &file);

	//locking the seek position in the openfile
	lock_acquire(file->of_offsetlock);

	//initializing a new iovec and uio for the kernel I/O
	uio_kinit(&myiov,&myuio, buf, size, file->of_offset, UIO_READ);

	result = VOP_READ(file->of_vnode, &myuio);
	file->of_offset = size + file->of_offset;
	openfile_incref(file);
	//locking the seek position in the openfile
	lock_release(file->of_offsetlock);
	filetable_put(curproc->p_filetable,fd,file);

       return result;
}

/*
 * write() - write data to a file
 */
int
sys_write(int fd, userptr_t buf, size_t size)
{
       int result = 0;
       struct openfile *file;
       struct uio myuio;
       struct iovec myiov;

       /*
        * Your implementation of system call read starts here.
        *
        * Check the design document design/filesyscall.txt for the steps
        */
	//grabbing the openfile from the filetable
	filetable_get(curproc->p_filetable, fd, &file);

	//locking the seek position in the openfile
	lock_acquire(file->of_offsetlock);

	//initializing a new iovec and uio for the kernel I/O
	uio_kinit(&myiov, &myuio, buf, size, file->of_offset, UIO_WRITE);

	result = VOP_WRITE(file->of_vnode, &myuio);

	file->of_offset = size+ file->of_offset;

	openfile_incref(file);

	//locking the seek position in the openfile
	lock_release(file->of_offsetlock);
	filetable_put(curproc->p_filetable,fd,file);


       return result;
}

/*
 * close() - remove from the file table.
 */
int
sys_close(int fd)
{
	struct openfile *oldfile;
	int result = 0;
	//validate fd number
	result = filetable_okfd(curproc->p_filetable, fd);
	if(!result){
	return -1;
	}
	//replace curproc's file table entry with null
	filetable_placeat(curproc->p_filetable, NULL, fd,&oldfile);

	//checking if the previous entry was null
	if(oldfile == NULL){
		return -1;
	}


	openfile_decref(oldfile);

	return 0;
}
/* 
* meld () - combine the content of two files word by word into a new file
*/
int sys_meld(const_userptr_t pn1,const_userptr_t pn2,const_userptr_t pn3){

	char *path1,*path2,*pathout;
/* *joined, *string1,*string2;*/
	int result,fd1=0,fd2=0,fdout=0,i;
	struct openfile *firstfile,*secondfile,*outputfile;
	userptr_t newfile1,newfile2;
	newfile1 = kmalloc(PATH_MAX);
	newfile2 = kmalloc(PATH_MAX);
	path1 = kmalloc(PATH_MAX);
	path2 = kmalloc(PATH_MAX);
	pathout = kmalloc(PATH_MAX);

	//copying in the supplied path names
	copyinstr(pn1,path1,PATH_MAX,NULL);
	copyinstr(pn2,path2,PATH_MAX,NULL);
	copyinstr(pn3,pathout,PATH_MAX,NULL);

	result = openfile_open(path1,O_RDONLY,0,&firstfile);
	if(result){return result;}

	result = openfile_open(path2,O_RDONLY,0,&secondfile);
	if(result){return result;}

	result = openfile_open(pathout,O_WRONLY|O_CREAT|O_APPEND,0,&outputfile);
	if(result){return result;}

	filetable_place(curproc->p_filetable,firstfile,&fd1);
	filetable_place(curproc->p_filetable,secondfile,&fd2);
	filetable_place(curproc->p_filetable,outputfile,&fdout);

	//use sys_read to get information
	for(i = 0;i<4;++i){
		sys_read(fd1,newfile1,4);
		sys_read(fd2,newfile2,4);
		sys_write(fdout,newfile1,4);
		sys_write(fdout,newfile2,4);

	}

	//use sys_close to close file
	sys_close(fd1);
	sys_close(fd2);
	sys_close(fdout);

	return 0;

}

