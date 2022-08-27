#include "jumbo_file_system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// C does not have a bool type, so I created one that you can use
typedef char bool_t;
#define TRUE 1
#define FALSE 0
#define dir 0
#define file 1

static block_num_t current_dir;

// optional helper function you can implement to tell you if a block is a dir node or an inode
static bool_t is_dir(block_num_t block_num)
{
  char buf[BLOCK_SIZE];
  if (read_block(block_num, buf) < 0)
  {
    return E_UNKNOWN;
  }
  struct block *blk = (struct block *)buf;
  if (blk->is_dir == dir)
  {
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

// isdir = 0, set block as directory; isdir=1, set block as regular file
static int set_dir(block_num_t block_num, int isdir)
{
  char buf[BLOCK_SIZE];
  if (read_block(block_num, buf) < 0)
  {
    return E_UNKNOWN;
  }
  struct block *blk = (struct block *)buf;
  blk->is_dir = isdir;
  if (isdir == dir)
  {
    (blk->contents).dirnode.num_entries = 0;
  }
  else
  {
    (blk->contents).inode.file_size = 0;
  }
  if (write_block(block_num, (void *)blk) < 0)
  {
    return E_UNKNOWN;
  }
  return E_SUCCESS;
}

// function is used to measure wheather the dir is empty;(required to confirm the block is dir before)
static bool_t is_dir_empty(block_num_t block_num)
{
  char buf[BLOCK_SIZE];
  if (read_block(block_num, buf) < 0)
  {
    return E_UNKNOWN;
  }
  struct block *blk = (struct block *)buf;
  if ((blk->contents).dirnode.num_entries == 0)
  {
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

/* jfs_mount
 *   prepares the DISK file on the _real_ file system to have file system
 *   blocks read and written to it.  The application _must_ call this function
 *   exactly once before calling any other jfs_* functions.  If your code
 *   requires any additional one-time initialization before any other jfs_*
 *   functions are called, you can add it here.
 * filename - the name of the DISK file on the _real_ file system
 * returns 0 on success or -1 on error; errors should only occur due to
 *   errors in the underlying disk syscalls.
 */
int jfs_mount(const char *filename)
{
  int ret = bfs_mount(filename);
  current_dir = 1;
  if (set_dir(current_dir, 0) < 0)
  {
    return E_UNKNOWN;
  }
  return ret;
}

/* jfs_mkdir
 *   creates a new subdirectory in the current directory
 * directory_name - name of the new subdirectory
 * returns 0 on success or one of the following error codes on failure:
 *   E_EXISTS, E_MAX_NAME_LENGTH, E_MAX_DIR_ENTRIES, E_DISK_FULL
 */
int jfs_mkdir(const char *directory_name)
{
  // allocate a block and set dir=1
  block_num_t next_dir = allocate_block();
  if (next_dir == 0)
  {
    return E_DISK_FULL;
  }
  if (set_dir(next_dir, 0) < 0)
  {
    return E_UNKNOWN;
  }

  // read the current dir block
  char buf[BLOCK_SIZE];
  if (read_block(current_dir, buf) < 0)
  {
    return E_UNKNOWN;
  }
  struct block *blk = (struct block *)buf;
  if (blk->is_dir != dir)
  {
    return E_UNKNOWN;
  }

  uint16_t num_entries = (blk->contents).dirnode.num_entries;
  if (num_entries == MAX_DIR_ENTRIES)
  {
    return E_MAX_DIR_ENTRIES;
  }
  // scan current block to see whether there are same directories
  int i;
  for (i = 0; i < num_entries; i++)
  {
    if (strcmp((blk->contents).dirnode.entries[i].name, directory_name) == 0)
    {
      return E_EXISTS;
    }
  }

  // set block_num
  (blk->contents).dirnode.entries[num_entries].block_num = next_dir;
  // set dir name
  int point = 0;
  while (*(directory_name + point) != '\0')
  {
    if (point >= MAX_NAME_LENGTH)
    {
      return E_MAX_NAME_LENGTH;
    }
    (blk->contents).dirnode.entries[num_entries].name[point] = *(directory_name + point);
    point++;
  }
  (blk->contents).dirnode.entries[num_entries].name[point] = '\0';
  // increase num_entries
  (blk->contents).dirnode.num_entries += 1;

  if (write_block(current_dir, (void *)blk) < 0)
  {
    return E_UNKNOWN;
  }
  return E_SUCCESS;
}

/* jfs_chdir
 *   changes the current directory to the specified subdirectory, or changes
 *   the current directory to the root directory if the directory_name is NULL
 * directory_name - name of the subdirectory to make the current
 *   directory; if directory_name is NULL then the current directory
 *   should be made the root directory instead
 * returns 0 on success or one of the following error codes on failure:
 *   E_NOT_EXISTS, E_NOT_DIR
 */
int jfs_chdir(const char *directory_name)
{
  if (directory_name == NULL)
  {
    // go back to root directory
    current_dir = 1;
    return E_SUCCESS;
  }
  // read the current dir block
  char buf[BLOCK_SIZE];
  if (read_block(current_dir, buf) < 0)
  {
    return E_UNKNOWN;
  }
  struct block *blk = (struct block *)buf;
  if (blk->is_dir != dir)
  {
    return E_UNKNOWN;
  }

  uint16_t num_entries = (blk->contents).dirnode.num_entries;
  int i;
  for (i = 0; i < num_entries; i++)
  {
    if (strcmp((blk->contents).dirnode.entries[i].name, directory_name) == 0)
    {
      block_num_t temp = (blk->contents).dirnode.entries[i].block_num;
      if (is_dir(temp) == FALSE)
      {
        return E_NOT_DIR;
      }
      else
      {
        current_dir = temp;
        return E_SUCCESS;
      }
    }
  }

  return E_NOT_EXISTS;
}

/* jfs_ls
 *   finds the names of all the files and directories in the current directory
 *   and writes the directory names to the directories argument and the file
 *   names to the files argument
 * directories - array of strings; the function will set the strings in the
 *   array, followed by a NULL pointer after the last valid string; the strings
 *   should be malloced and the caller will free them
 * file - array of strings; the function will set the strings in the
 *   array, followed by a NULL pointer after the last valid string; the strings
 *   should be malloced and the caller will free them
 * returns 0 on success or one of the following error codes on failure:
 *   (this function should always succeed)
 */
int jfs_ls(char *directories[MAX_DIR_ENTRIES + 1], char *files[MAX_DIR_ENTRIES + 1])
{
  // read the current dir block
  char buf[BLOCK_SIZE];
  if (read_block(current_dir, buf) < 0)
  {
    return E_UNKNOWN;
  }
  struct block *blk = (struct block *)buf;
  if (blk->is_dir != dir)
  {
    return E_UNKNOWN;
  }

  uint16_t num_entries = (blk->contents).dirnode.num_entries;
  int dir_point = 0;
  int file_point = 0;
  int i;
  for (i = 0; i < num_entries; i++)
  {
    int name_len = strlen((blk->contents).dirnode.entries[i].name);
    char *str = (char *)malloc(name_len + 1);
    strcpy(str, (blk->contents).dirnode.entries[i].name);
    if (is_dir((blk->contents).dirnode.entries[i].block_num) == TRUE)
    {
      directories[dir_point++] = str;
    }
    else
    {
      files[file_point++] = str;
    }
  }
  directories[dir_point] = NULL;
  files[file_point] = NULL;

  return E_SUCCESS;
}

/* jfs_rmdir
 *   removes the specified subdirectory of the current directory
 * directory_name - name of the subdirectory to remove
 * returns 0 on success or one of the following error codes on failure:
 *   E_NOT_EXISTS, E_NOT_DIR, E_NOT_EMPTY
 */
int jfs_rmdir(const char *directory_name)
{
  // read the current dir block
  char buf[BLOCK_SIZE];
  if (read_block(current_dir, buf) < 0)
  {
    return E_UNKNOWN;
  }
  struct block *blk = (struct block *)buf;
  if (blk->is_dir != dir)
  {
    return E_UNKNOWN;
  }

  uint16_t num_entries = (blk->contents).dirnode.num_entries;
  // scan current block to see whether there is required directory
  int i;
  for (i = 0; i < num_entries; i++)
  {
    if (strcmp((blk->contents).dirnode.entries[i].name, directory_name) == 0)
    {
      block_num_t temp = (blk->contents).dirnode.entries[i].block_num;
      if (is_dir(temp) == FALSE)
      {
        return E_NOT_DIR;
      }
      if (is_dir_empty(temp) == FALSE)
      {
        return E_NOT_EMPTY;
      }

      // delete directory
      int j;
      for (j = i; j < (num_entries - 1); j++)
      {
        // (blk->contents).dirnode.entries[j].block_num = (blk->contents).dirnode.entries[j + 1].block_num;
        // (blk->contents).dirnode.entries[j].name = (blk->contents).dirnode.entries[j + 1].name;
        (blk->contents).dirnode.entries[j] = (blk->contents).dirnode.entries[j + 1];
      }
      (blk->contents).dirnode.num_entries -= 1;
      if (write_block(current_dir, (void *)blk) < 0)
      {
        return E_UNKNOWN;
      }
      if (release_block(temp) < 0)
      {
        return E_UNKNOWN;
      }
      return E_SUCCESS;
    }
  }

  return E_NOT_EXISTS;
}

/* jfs_creat
 *   creates a new, empty file with the specified name
 * file_name - name to give the new file
 * returns 0 on success or one of the following error codes on failure:
 *   E_EXISTS, E_MAX_NAME_LENGTH, E_MAX_DIR_ENTRIES, E_DISK_FULL
 */
int jfs_creat(const char *file_name)
{
  // allocate a block and set dir=1
  block_num_t next_file = allocate_block();
  if (next_file == 0)
  {
    return E_DISK_FULL;
  }
  if (set_dir(next_file, 1) < 0)
  {
    return E_UNKNOWN;
  }

  // read the current dir block
  char buf[BLOCK_SIZE];
  if (read_block(current_dir, buf) < 0)
  {
    return E_UNKNOWN;
  }
  struct block *blk = (struct block *)buf;
  if (blk->is_dir != dir)
  {
    return E_UNKNOWN;
  }

  uint16_t num_entries = (blk->contents).dirnode.num_entries;
  if (num_entries == MAX_DIR_ENTRIES)
  {
    return E_MAX_DIR_ENTRIES;
  }
  // scan current block to see whether there are same file
  int i;
  for (i = 0; i < num_entries; i++)
  {
    if (strcmp((blk->contents).dirnode.entries[i].name, file_name) == 0)
    {
      return E_EXISTS;
    }
  }

  int file_len = strlen(file_name);
  if (file_len > MAX_NAME_LENGTH)
  {
    return E_MAX_NAME_LENGTH;
  }

  // set file information
  (blk->contents).dirnode.entries[num_entries].block_num = next_file;
  strcpy((blk->contents).dirnode.entries[num_entries].name, file_name);
  // increase num_entries
  (blk->contents).dirnode.num_entries += 1;

  if (write_block(current_dir, (void *)blk) < 0)
  {
    return E_UNKNOWN;
  }
  return E_SUCCESS;
}

/* jfs_remove
 *   deletes the specified file and all its data (note that this cannot delete
 *   directories; use rmdir instead to remove directories)
 * file_name - name of the file to remove
 * returns 0 on success or one of the following error codes on failure:
 *   E_NOT_EXISTS, E_IS_DIR
 */
int jfs_remove(const char *file_name)
{
  // read the current dir block
  char buf[BLOCK_SIZE];
  if (read_block(current_dir, buf) < 0)
  {
    return E_UNKNOWN;
  }
  struct block *blk = (struct block *)buf;
  if (blk->is_dir != dir)
  {
    return E_UNKNOWN;
  }

  uint16_t num_entries = (blk->contents).dirnode.num_entries;
  // scan current block to see whether there is required file
  int i;
  for (i = 0; i < num_entries; i++)
  {
    if (strcmp((blk->contents).dirnode.entries[i].name, file_name) == 0)
    {
      block_num_t temp = (blk->contents).dirnode.entries[i].block_num;
      if (is_dir(temp) == TRUE)
      {
        return E_IS_DIR;
      }

      // delete file
      int j;
      for (j = i; j < (num_entries - 1); j++)
      {
        // (blk->contents).dirnode.entries[j].block_num = (blk->contents).dirnode.entries[j + 1].block_num;
        // (blk->contents).dirnode.entries[j].name = (blk->contents).dirnode.entries[j + 1].name;
        (blk->contents).dirnode.entries[j] = (blk->contents).dirnode.entries[j + 1];
      }
      (blk->contents).dirnode.num_entries -= 1;
      if (write_block(current_dir, (void *)blk) < 0)
      {
        return E_UNKNOWN;
      }

      // read the file block
      char file_buf[BLOCK_SIZE];
      if (read_block(temp, file_buf) < 0)
      {
        return E_UNKNOWN;
      }
      struct block *blk_f = (struct block *)file_buf;
      if ((blk_f->contents).inode.file_size != 0)
      {
        // the file is not empty, so we need to release relevant block
        uint32_t f_size = (blk_f->contents).inode.file_size;
        int file_len = (blk_f->contents).inode.file_size / BLOCK_SIZE;
        if (f_size % BLOCK_SIZE != 0)
          file_len += 1;
        int fl;
        for (fl = 0; fl < file_len; fl++)
        {
          if (release_block((blk_f->contents).inode.data_blocks[fl]) < 0)
          {
            return E_UNKNOWN;
          }
        }
      }

      if (release_block(temp) < 0)
      {
        return E_UNKNOWN;
      }
      return E_SUCCESS;
    }
  }

  return E_NOT_EXISTS;
}

/* jfs_stat
 *   returns the file or directory stats (see struct stat for details)
 * name - name of the file or directory to inspect
 * buf  - pointer to a struct stat (already allocated by the caller) where the
 *   stats will be written
 * returns 0 on success or one of the following error codes on failure:
 *   E_NOT_EXISTS
 */
int jfs_stat(const char *name, struct stats *buf)
{
  // read the current dir block
  char dir_buf[BLOCK_SIZE];
  if (read_block(current_dir, dir_buf) < 0)
  {
    return E_UNKNOWN;
  }
  struct block *blk = (struct block *)dir_buf;
  if (blk->is_dir != dir)
  {
    return E_UNKNOWN;
  }

  uint16_t num_entries = (blk->contents).dirnode.num_entries;
  int i;
  for (i = 0; i < num_entries; i++)
  {
    if (strcmp((blk->contents).dirnode.entries[i].name, name) == 0)
    {
      block_num_t temp = (blk->contents).dirnode.entries[i].block_num;
      // set name and block_num_t
      buf->block_num = temp;
      strcpy(buf->name, name);

      // read the file block
      char temp_buf[BLOCK_SIZE];
      if (read_block(temp, temp_buf) < 0)
      {
        return E_UNKNOWN;
      }
      struct block *blk_temp = (struct block *)temp_buf;
      if (blk_temp->is_dir == dir)
      {
        buf->is_dir = dir;
        return E_SUCCESS;
      }
      else
      {
        buf->is_dir = file;
        buf->file_size = (blk_temp->contents).inode.file_size;
        int blk_len = (blk_temp->contents).inode.file_size / BLOCK_SIZE;
        if ((blk_temp->contents).inode.file_size % BLOCK_SIZE != 0)
          blk_len += 1;
        buf->num_data_blocks = blk_len;
        return E_SUCCESS;
      }
    }
  }

  return E_NOT_EXISTS;
}

/* jfs_write
 *   appends the data in the buffer to the end of the specified file
 * file_name - name of the file to append data to
 * buf - buffer containing the data to be written (note that the data could be
 *   binary, not text, and even if it is text should not be assumed to be null
 *   terminated)
 * count - number of bytes in buf (write exactly this many)
 * returns 0 on success or one of the following error codes on failure:
 *   E_NOT_EXISTS, E_IS_DIR, E_MAX_FILE_SIZE, E_DISK_FULL
 */
int jfs_write(const char *file_name, const void *buf, unsigned short count)
{
  // read the current dir block
  char dir_buf[BLOCK_SIZE];
  if (read_block(current_dir, dir_buf) < 0)
  {
    return E_UNKNOWN;
  }
  struct block *blk = (struct block *)dir_buf;
  if (blk->is_dir != dir)
  {
    return E_UNKNOWN;
  }

  uint16_t num_entries = (blk->contents).dirnode.num_entries;
  int i;
  for (i = 0; i < num_entries; i++)
  {
    if (strcmp((blk->contents).dirnode.entries[i].name, file_name) == 0)
    {
      block_num_t temp = (blk->contents).dirnode.entries[i].block_num;
      // read the file block
      char temp_buf[BLOCK_SIZE];
      if (read_block(temp, temp_buf) < 0)
      {
        return E_UNKNOWN;
      }
      // blk_temp is the point to dirnode  that we are looking for
      struct block *blk_temp = (struct block *)temp_buf;
      if (blk_temp->is_dir == dir)
      {
        return E_IS_DIR;
      }

      // calculate the file size that we need to append
      uint32_t fz = (blk_temp->contents).inode.file_size;
      if (fz + count > MAX_FILE_SIZE)
        return E_MAX_FILE_SIZE;
      // calculate the block that we need to allocate
      int more_blk;
      bool_t need_blk = TRUE;
      if (fz % BLOCK_SIZE != 0)
      {
        int last_blk_ava = BLOCK_SIZE - (fz % BLOCK_SIZE);
        if (count <= last_blk_ava)
        {
          // no need to apply new block
          more_blk = 1;
          need_blk = FALSE;
        }
        else
        {
          // need to apply new block and calculate
          more_blk = (count - (BLOCK_SIZE - (fz % BLOCK_SIZE))) / BLOCK_SIZE;
          if ((count - (BLOCK_SIZE - (fz % BLOCK_SIZE))) % BLOCK_SIZE != 0)
            more_blk += 1;
        }
      }
      else
      {
        more_blk = count / BLOCK_SIZE;
        if (count % BLOCK_SIZE != 0)
          more_blk += 1;
      }
      block_num_t new_blk_arr[more_blk];
      if (need_blk == TRUE)
      {
        int j;
        for (j = 0; j < more_blk; j++)
        {
          block_num_t next_blk = allocate_block();
          if (next_blk == 0)
          {
            int k;
            for (k = j - 1; k >= 0; k--)
            {
              release_block(new_blk_arr[k]);
            }
            return E_DISK_FULL;
          }
          else
          {
            new_blk_arr[j] = next_blk;
          }
        }
      }

      // written_byte is the length of bytes that required to append
      int written_byte = count;
      char *res_buf = (char *)buf;

      // start to write data to file block
      // first, check whether there is block that not full
      int blk_len = fz / BLOCK_SIZE;
      if (fz % BLOCK_SIZE != 0)
      {
        // append to last block, which is not full
        block_num_t last = (blk_temp->contents).inode.data_blocks[blk_len];
        int used = fz % BLOCK_SIZE;
        int left = BLOCK_SIZE - used;
        char prev_buf[BLOCK_SIZE];
        if (read_block(last, prev_buf) < 0)
        {
          return E_UNKNOWN;
        }
        int temp_write_len = written_byte - left >= 0 ? left : written_byte;
        memcpy(prev_buf + used, res_buf, temp_write_len);
        written_byte -= temp_write_len;
        if (write_block(last, (void *)prev_buf) < 0)
        {
          return E_UNKNOWN;
        }
        blk_len++;
      }

      if (need_blk == TRUE)
      {
        // allocate a new block to store the data
        int blk_point;
        int buf_point = count - written_byte;
        for (blk_point = 0; blk_point < more_blk; blk_point++)
        {
          block_num_t wrt_blk = new_blk_arr[blk_point];
          (blk_temp->contents).inode.data_blocks[blk_len] = wrt_blk;
          char write_buf[BLOCK_SIZE];
          memset(write_buf, -1, BLOCK_SIZE);
          int write_len = written_byte >= BLOCK_SIZE ? BLOCK_SIZE : written_byte;
          // int p;
          // for (p = 0; p < write_len; p++)
          // {
          //   write_buf[p] = res_buf[p + buf_point];
          //   // printf("the write char is : %c\n", (char)res_buf[p + buf_point]);
          // }
          memcpy(write_buf, res_buf + buf_point, write_len);
          if (write_block(wrt_blk, (void *)write_buf) < 0)
          {
            return E_UNKNOWN;
          }
          written_byte -= write_len;
          buf_point += write_len;
          blk_len++;
        }
      }

      // write back the file inode information
      (blk_temp->contents).inode.file_size += count;
      if (write_block(temp, (void *)blk_temp) < 0)
      {
        return E_UNKNOWN;
      }

      return E_SUCCESS;
    }
  }

  return E_NOT_EXISTS;
}

/* jfs_read
 *   reads the specified file and copies its contents into the buffer, up to a
 *   maximum of *ptr_count bytes copied (but obviously no more than the file
 *   size, either)
 * file_name - name of the file to read
 * buf - buffer where the file data should be written
 * ptr_count - pointer to a count variable (allocated by the caller) that
 *   contains the size of buf when it's passed in, and will be modified to
 *   contain the number of bytes actually written to buf (e.g., if the file is
 *   smaller than the buffer) if this function is successful
 * returns 0 on success or one of the following error codes on failure:
 *   E_NOT_EXISTS, E_IS_DIR
 */
int jfs_read(const char *file_name, void *buf, unsigned short *ptr_count)
{
  // read the current dir block
  char dir_buf[BLOCK_SIZE];
  if (read_block(current_dir, dir_buf) < 0)
  {
    return E_UNKNOWN;
  }
  struct block *blk = (struct block *)dir_buf;
  if (blk->is_dir != dir)
  {
    return E_UNKNOWN;
  }

  uint16_t num_entries = (blk->contents).dirnode.num_entries;
  int i;
  for (i = 0; i < num_entries; i++)
  {
    if (strcmp((blk->contents).dirnode.entries[i].name, file_name) == 0)
    {
      block_num_t temp = (blk->contents).dirnode.entries[i].block_num;
      // read the file block
      char temp_buf[BLOCK_SIZE];
      if (read_block(temp, temp_buf) < 0)
      {
        return E_UNKNOWN;
      }
      // blk_temp is the point to inode  that we are looking for
      struct block *blk_temp = (struct block *)temp_buf;
      if (blk_temp->is_dir == dir)
      {
        return E_IS_DIR;
      }

      uint32_t fz = (blk_temp->contents).inode.file_size;
      char *res_buf = (char *)buf;
      // start to read data from file block
      // first, calculate the number of blocks the file has
      int blk_len = fz / BLOCK_SIZE;
      if (fz % BLOCK_SIZE != 0)
        blk_len += 1;

      unsigned short read_num = *ptr_count > fz ? fz : *ptr_count;
      int buf_point = 0;
      int bk;
      for (bk = 0; bk < blk_len; bk++)
      {
        block_num_t read_blk = (blk_temp->contents).inode.data_blocks[bk];
        // printf("block_num_t is %d\n", read_blk);
        char read_buf[BLOCK_SIZE];
        if (read_block(read_blk, read_buf) < 0)
        {
          return E_UNKNOWN;
        }

        int read_len = read_num >= BLOCK_SIZE ? BLOCK_SIZE : read_num;
        // printf("read_len is %d\n", read_len);
        //  int p;
        //  for (p = 0; p < read_len; p++)
        //  {
        //    // printf("read data is %c\n", read_buf[p]);
        //    res_buf[p + buf_point] = read_buf[p];
        //  }
        memcpy(res_buf + buf_point, read_buf, read_len);
        read_num -= read_len;
        buf_point += read_len;
      }
      // res_buf[buf_point] = '\0';
      *ptr_count = *ptr_count > fz ? fz : *ptr_count;

      return E_SUCCESS;
    }
  }

  return E_NOT_EXISTS;
}

/* jfs_unmount
 *   makes the file system no longer accessible (unless it is mounted again).
 *   This should be called exactly once after all other jfs_* operations are
 *   complete; it is invalid to call any other jfs_* function (except
 *   jfs_mount) after this function complete.  Basically, this closes the DISK
 *   file on the _real_ file system.  If your code requires any clean up after
 *   all other jfs_* functions are done, you may add it here.
 * returns 0 on success or -1 on error; errors should only occur due to
 *   errors in the underlying disk syscalls.
 */
int jfs_unmount()
{
  int ret = bfs_unmount();
  return ret;
}
