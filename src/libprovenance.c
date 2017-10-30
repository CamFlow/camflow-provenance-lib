/*
*
* Author: Thomas Pasquier <tfjmp2@cam.ac.uk>
*
* Copyright (C) 2015 University of Cambridge
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2, as
* published by the Free Software Foundation.
*
*/
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/xattr.h>
#include <linux/xattr.h>
#include <pwd.h>
#include <grp.h>
#include <linux/provenance_types.h>

#include "provenance.h"
#include "provenanceutils.h"
#include "uthash.h"

static inline int __set_boolean(bool value, const char* name){
  int rc;
  int fd = open(name, O_WRONLY);

  if(fd<0)
  {
    return fd;
  }
  if(value)
  {
    rc = write(fd, "1", sizeof(char));
  }else{
    rc = write(fd, "0", sizeof(char));
  }
  close(fd);
  if(rc < 0){
    return rc;
  }
  return 0;
}

static inline bool __get_boolean(const char* name){
  int fd = open(name, O_RDONLY);
  int rc;
  char c;
  if(fd<0)
  {
    return false;
  }

  rc = read(fd, &c, sizeof(char));
  close(fd);
  if( rc<0 ){
    return false;
  }
  return c!='0';
}

#define declare_set_boolean_fcn( fcn_name, file_name ) int fcn_name (bool value ) { return __set_boolean(value, file_name);}
#define declare_get_boolean_fcn( fcn_name, file_name ) bool fcn_name ( void ) { return __get_boolean(file_name);}

declare_set_boolean_fcn(provenance_set_enable, PROV_ENABLE_FILE);
declare_get_boolean_fcn(provenance_get_enable, PROV_ENABLE_FILE);

declare_set_boolean_fcn(provenance_set_all, PROV_ALL_FILE);
declare_get_boolean_fcn(provenance_get_all, PROV_ALL_FILE);

declare_set_boolean_fcn(provenance_should_compress, PROV_COMPRESS_FILE);
declare_get_boolean_fcn(provenance_does_compress, PROV_COMPRESS_FILE);

#define declare_self_set_flag(fcn_name, element, operation) int fcn_name (bool v){ \
  struct prov_process_config cfg;\
  int rc;\
  int fd = open(PROV_SELF_FILE, O_WRONLY);\
  if( fd < 0 ){\
    return fd;\
  }\
  memset(&cfg, 0, sizeof(struct prov_process_config));\
  cfg.op=operation;\
  if(v){\
    prov_set_flag(&cfg.prov, element);\
  }else{\
    prov_clear_flag(&cfg.prov, element);\
  }\
  rc = write(fd, &cfg, sizeof(struct prov_process_config));\
  close(fd);\
  if(rc>0) rc=0;\
  return rc;\
}

#define declare_self_get_flag(fcn_name, element) bool fcn_name( void ){\
  union prov_elt self;\
  provenance_self(&self.task_info);\
  return prov_check_flag(&self, element);\
}

declare_self_set_flag(provenance_set_tracked, TRACKED_BIT, PROV_SET_TRACKED);
declare_self_get_flag(provenance_get_tracked, TRACKED_BIT);

declare_self_set_flag(provenance_set_opaque, OPAQUE_BIT, PROV_SET_OPAQUE);
declare_self_get_flag(provenance_get_opaque, OPAQUE_BIT);

declare_self_set_flag(__provenance_set_propagate, PROPAGATE_BIT, PROV_SET_PROPAGATE);
declare_self_get_flag(provenance_get_propagate, PROPAGATE_BIT);

int provenance_set_propagate(bool v){
  int err;
  err = __provenance_set_propagate(v);
  if(err < 0)
    return err;
  return provenance_set_tracked(v);
}

int provenance_set_machine_id(uint32_t v){
  int rc;
  int fd = open(PROV_MACHINE_ID_FILE, O_WRONLY);

  if(fd<0)
    return fd;
  rc = write(fd, &v, sizeof(uint32_t));
  close(fd);
  if(rc<0)
    return rc;
  return 0;
}

int provenance_get_machine_id(uint32_t* v){
  int rc;
  int fd = open(PROV_MACHINE_ID_FILE, O_RDONLY);

  if(fd<0)
    return fd;
  rc = read(fd, v, sizeof(uint32_t));
  close(fd);
  if(rc<0)
    return rc;
  return 0;
}

int provenance_set_boot_id(uint32_t v){
  int rc;
  int fd = open(PROV_BOOT_ID_FILE, O_WRONLY);

  if(fd<0)
    return fd;
  rc = write(fd, &v, sizeof(uint32_t));
  close(fd);
  if(rc<0)
    return rc;
  return 0;
}

int provenance_get_boot_id(uint32_t* v){
  int rc;
  int fd = open(PROV_BOOT_ID_FILE, O_RDONLY);

  if(fd<0)
    return fd;
  rc = read(fd, v, sizeof(uint32_t));
  close(fd);
  if(rc<0)
    return rc;
  return 0;
}

int provenance_disclose_node(struct disc_node_struct* node){
  int rc;
  int fd = open(PROV_NODE_FILE, O_WRONLY);

  if(fd<0)
    return fd;
  rc = write(fd, node, sizeof(struct disc_node_struct));
  close(fd);
  return rc;
}

int provenance_disclose_relation(struct relation_struct* relation){
  int rc;
  int fd = open(PROV_RELATION_FILE, O_WRONLY);

  if(fd<0)
    return fd;
  rc = write(fd, relation, sizeof(struct relation_struct));
  close(fd);
  return rc;
}

int provenance_self(struct task_prov_struct* self){
  int rc;
  int fd = open(PROV_SELF_FILE, O_RDONLY);

  if(fd<0)
    return fd;
  rc = read(fd, self, sizeof(struct task_prov_struct));
  close(fd);
  return rc;
}

bool provenance_is_present(void){
  if(access(PROV_ENABLE_FILE, F_OK)) // return 0 if file exists.
    return false;
  return true;
}

int provenance_flush(void){
  char tmp = 1;
  int rc;
  int fd = open(PROV_FLUSH_FILE, O_WRONLY);

  if(fd<0)
    return fd;
  rc = write(fd, &tmp, sizeof(char));
  close(fd);
  return rc;
}

int provenance_read_file(const char path[PATH_MAX], union prov_elt* inode_info){
  return getxattr(path, XATTR_NAME_PROVENANCE, inode_info, sizeof(union prov_elt));
}

int provenance_file_id(const char path[PATH_MAX], char* buff, size_t len){
  int rc;
  union prov_elt inode_info;
  char id[PROV_ID_STR_LEN];

  if(len < PROV_ID_STR_LEN)
    return -ENOMEM;

  rc = getxattr(path, XATTR_NAME_PROVENANCE, &inode_info, sizeof(union prov_elt));
  if(rc < 0)
    return rc;
    rc = ID_ENCODE(prov_id_buffer(&inode_info), PROV_IDENTIFIER_BUFFER_LENGTH, id, PROV_ID_STR_LEN);
    if (rc < 0)
      return rc;
    sprintf(buff, "cf:%s", id);
    return 0;
}

int fprovenance_read_file(int fd, union prov_elt* inode_info){
  return fgetxattr(fd, XATTR_NAME_PROVENANCE, inode_info, sizeof(union prov_elt));
}

int fprovenance_file_id(int fd, char* buff, size_t len){
  int rc;
  union prov_elt inode_info;
  char id[PROV_ID_STR_LEN];

  if(len < PROV_ID_STR_LEN+3)
    return -ENOMEM;

  rc = fgetxattr(fd, XATTR_NAME_PROVENANCE, &inode_info, sizeof(union prov_elt));
  if (rc < 0)
    return rc;
  rc = ID_ENCODE(prov_id_buffer(&inode_info), PROV_IDENTIFIER_BUFFER_LENGTH, id, PROV_ID_STR_LEN);
  if (rc < 0)
    return rc;
  sprintf(buff, "cf:%s", id);
  return 0;
}

static inline int __provenance_write_file(const char path[PATH_MAX], union prov_elt* inode_info){
  return setxattr(path, XATTR_NAME_PROVENANCE, inode_info, sizeof(union prov_elt), 0);
}

static inline int __fprovenance_write_file(int fd, union prov_elt* inode_info){
  return fsetxattr(fd, XATTR_NAME_PROVENANCE, inode_info, sizeof(union prov_elt), 0);
}

static inline int __provenance_set_flags_file(const char path[PATH_MAX], uint8_t bit, bool v){
  union prov_elt prov;
  int rc;
  rc = provenance_read_file(path, &prov);
  if(rc<0)
    return rc;
  if(v)
    prov_set_flag(&prov, bit);
  else
    prov_clear_flag(&prov, bit);
  return __provenance_write_file(path, &prov);
}

static inline int __fprovenance_set_flags_file(int fd, uint8_t bit, bool v){
  union prov_elt prov;
  int rc;
  rc = fprovenance_read_file(fd, &prov);
  if(rc<0)
    return rc;
  if(v)
    prov_set_flag(&prov, bit);
  else
    prov_clear_flag(&prov, bit);
  return __fprovenance_write_file(fd, &prov);
}

#define declare_set_file_fcn(fcn_name, element) int fcn_name (const char name[PATH_MAX], bool v){\
    return __provenance_set_flags_file(name, element, v);\
  }

#define declare_fset_file_fcn(fcn_name, element) int fcn_name (int fd, bool v){\
    return __fprovenance_set_flags_file(fd, element, v);\
  }

declare_set_file_fcn(provenance_track_file, TRACKED_BIT);
declare_set_file_fcn(provenance_opaque_file, OPAQUE_BIT);
declare_set_file_fcn(__provenance_propagate_file, PROPAGATE_BIT);

declare_fset_file_fcn(fprovenance_track_file, TRACKED_BIT);
declare_fset_file_fcn(fprovenance_opaque_file, OPAQUE_BIT);
declare_fset_file_fcn(__fprovenance_propagate_file, PROPAGATE_BIT);

int provenance_propagate_file(const char name[PATH_MAX], bool propagate){
  int err;
  err = __provenance_propagate_file(name, propagate);
  if(err < 0)
    return err;
  return provenance_track_file(name, propagate);
}

int fprovenance_propagate_file(int fd, bool propagate){
  int err;
  err = __fprovenance_propagate_file(fd, propagate);
  if(err < 0)
    return err;
  return fprovenance_track_file(fd, propagate);
}

int provenance_label_file(const char path[PATH_MAX], const char *label){
  union prov_elt prov;
  uint64_t taint = generate_label(label);
  int rc;
  rc = provenance_read_file(path, &prov);
  if(rc<0)
    return rc;
  prov_bloom_add(prov_taint(&prov), taint);
  return __provenance_write_file(path, &prov);
}

int fprovenance_label_file(int fd, const char *label){
  union prov_elt prov;
  uint64_t taint = generate_label(label);
  int rc;
  rc = fprovenance_read_file(fd, &prov);
  if(rc<0)
    return rc;
  prov_bloom_add(prov_taint(&prov), taint);
  return __fprovenance_write_file(fd, &prov);
}

int provenance_label(const char *label){
  struct prov_process_config cfg;
  uint64_t taint = generate_label(label);
  int rc;
  int fd = open(PROV_SELF_FILE, O_WRONLY);
  if( fd < 0 )
    return fd;
  memset(&cfg, 0, sizeof(struct prov_process_config));
  cfg.op=PROV_SET_TAINT;
  prov_bloom_add(prov_taint(&(cfg.prov)), taint);

  rc = write(fd, &cfg, sizeof(struct prov_process_config));
  close(fd);
  return rc;
}

int provenance_read_process(uint32_t pid, union prov_elt* process_info){
  struct prov_process_config cfg;
  int rc;
  int fd = open(PROV_PROCESS_FILE, O_RDONLY);

  if( fd < 0 )
    return fd;
  cfg.vpid = pid;

  rc = read(fd, &cfg, sizeof(struct prov_process_config));
  close(fd);
  memcpy(process_info, &(cfg.prov), sizeof(union prov_elt));
  return rc;
}

#define declare_set_process_fcn(fcn_name, element, operation) int fcn_name (uint32_t pid, bool v){\
    struct prov_process_config cfg;\
    int rc;\
    int fd = open(PROV_PROCESS_FILE, O_WRONLY);\
    if( fd < 0 ){\
      return fd;\
    }\
    cfg.vpid = pid;\
    cfg.op=operation;\
    if(v){\
      prov_set_flag(&cfg.prov, element);\
    }else{\
      prov_clear_flag(&cfg.prov, element);\
    }\
    rc = write(fd, &cfg, sizeof(struct prov_process_config));\
    close(fd);\
    return rc;\
  }

declare_set_process_fcn(provenance_track_process, TRACKED_BIT, PROV_SET_TRACKED);
declare_set_process_fcn(provenance_opaque_process, OPAQUE_BIT, PROV_SET_OPAQUE);
declare_set_process_fcn(__provenance_propagate_process, PROPAGATE_BIT, PROV_SET_PROPAGATE);

int provenance_propagate_process(uint32_t pid, bool propagate){
  int err;
  err = __provenance_propagate_process(pid, propagate);
  if(err < 0)
    return err;
  return provenance_track_process(pid, propagate);
}

int provenance_label_process(uint32_t pid, const char *label){
  struct prov_process_config cfg;
  uint64_t taint = generate_label(label);
  int rc;
  int fd = open(PROV_PROCESS_FILE, O_WRONLY);
  if( fd < 0 )
    return fd;
  memset(&cfg, 0, sizeof(struct prov_process_config));
  cfg.vpid=pid;
  cfg.op=PROV_SET_TAINT;
  prov_bloom_add(prov_taint(&(cfg.prov)), taint);

  rc = write(fd, &cfg, sizeof(struct prov_process_config));
  close(fd);
  return rc;
}

union ipaddr{
  uint32_t value;
  uint8_t buffer[4];
};

static int __param_to_ipv4_filter(const char* param, struct prov_ipv4_filter* filter){
  int err;
  union ipaddr ip;
  uint32_t a;
  uint32_t b;
  uint32_t c;
  uint32_t d;
  uint32_t mask;
  uint32_t port;

  err = sscanf(param, "%u.%u.%u.%u/%u:%u", &a, &b, &c, &d, &mask, &port);
  ip.buffer[0]=a;
  ip.buffer[1]=b;
  ip.buffer[2]=c;
  ip.buffer[3]=d;
  if(err < 6){
    errno=-EINVAL;
    return -EINVAL;
  }

  if(port > 65535 || mask > 32){
    errno=-EINVAL;
    return -EINVAL;
  }
  mask = uint32_to_ipv4mask(mask);
  filter->ip=ip.value;
  filter->mask=mask;
  filter->port=htons(port);
  return 0;
}

#define declare_set_ipv4_fcn(fcn_name, file, operation) int fcn_name (const char* param){\
  struct prov_ipv4_filter filter;\
  int rc;\
  int fd = open(file, O_WRONLY);\
  if( fd < 0 ){\
    return fd;\
  }\
  rc = __param_to_ipv4_filter(param, &filter);\
  if(rc!=0){\
    return rc;\
  }\
  filter.op = operation;\
  rc = write(fd, &filter, sizeof(struct prov_ipv4_filter));\
  close(fd);\
  return rc;\
}

#define declare_get_ipv4_fcn(fcn_name, file) int fcn_name ( struct prov_ipv4_filter* filters, size_t length ){\
  int rc;\
  int fd = open(file, O_RDONLY);\
  if( fd < 0 ){\
    return fd;\
  }\
  rc = read(fd, filters, length);\
  close(fd);\
  return rc;\
}

declare_set_ipv4_fcn(provenance_ingress_ipv4_track, PROV_IPV4_INGRESS_FILE, PROV_SET_TRACKED);
declare_set_ipv4_fcn(provenance_ingress_ipv4_propagate, PROV_IPV4_INGRESS_FILE, PROV_SET_TRACKED|PROV_SET_PROPAGATE);
declare_set_ipv4_fcn(provenance_ingress_ipv4_record, PROV_IPV4_INGRESS_FILE, PROV_SET_TRACKED|PROV_SET_RECORD);
declare_set_ipv4_fcn(provenance_ingress_ipv4_delete, PROV_IPV4_INGRESS_FILE, PROV_SET_DELETE);

declare_set_ipv4_fcn(provenance_egress_ipv4_track, PROV_IPV4_EGRESS_FILE, PROV_SET_TRACKED);
declare_set_ipv4_fcn(provenance_egress_ipv4_propagate, PROV_IPV4_EGRESS_FILE, PROV_SET_TRACKED|PROV_SET_PROPAGATE);
declare_set_ipv4_fcn(provenance_egress_ipv4_record, PROV_IPV4_EGRESS_FILE, PROV_SET_TRACKED|PROV_SET_RECORD);
declare_set_ipv4_fcn(provenance_egress_ipv4_delete, PROV_IPV4_EGRESS_FILE, PROV_SET_DELETE);

declare_get_ipv4_fcn(provenance_ingress_ipv4, PROV_IPV4_INGRESS_FILE);
declare_get_ipv4_fcn(provenance_egress_ipv4, PROV_IPV4_EGRESS_FILE);

struct secentry {
    int id;            /* we'll use this field as the key */
    char name[200];
    UT_hash_handle hh; /* makes this structure hashable */
};

static __thread struct secentry *hash = NULL;

bool sec_exists_entry(uint32_t secid) {
  struct secentry *se=NULL;
  HASH_FIND_INT(hash, &secid, se);
  if(!se)
    return false;
  return true;
}

static void sec_add_entry(uint32_t secid, const char* secctx){
  struct secentry *se;
  if( sec_exists_entry(secid) )
    return;
  se = malloc(sizeof(struct secentry));
  se->id=secid;
  strncpy(se->name, secctx, 200);
  HASH_ADD_INT(hash, id, se);
}

bool sec_find_entry(uint32_t secid, char* secctx) {
  struct secentry *se=NULL;
  HASH_FIND_INT(hash, &secid, se);
  if(!se)
    return false;
  strncpy(secctx, se->name, 200);
  return true;
}

int provenance_secid_to_secctx( uint32_t secid, char* secctx, uint32_t len){
  struct secinfo info;
  int rc;
  int fd;

  if( sec_find_entry(secid, secctx) )
    return 0;
  fd = open(PROV_SECCTX, O_RDONLY);
  if( fd < 0 )
    return fd;
  memset(&info, 0, sizeof(struct secinfo));
  info.secid=secid;
  rc = read(fd, &info, sizeof(struct secinfo));
  close(fd);
  if(rc<0){
    secctx[0]='\0';
    return rc;
  }
  if(len<strlen(info.secctx))
    return -ENOMEM;
  strncpy(secctx, info.secctx, len);
  sec_add_entry(secid, secctx);
  return rc;
}

struct typeentry {
    uint64_t id;
    char str[256];
    UT_hash_handle hh; /* makes this structure hashable */
};

static __thread struct typeentry *thash = NULL;

bool type_exists_entry(uint64_t typeid) {
  struct typeentry *te=NULL;
  HASH_FIND(hh, thash, &typeid, sizeof(uint64_t), te);
  if(!te)
    return false;
  return true;
}

static void type_add_entry(uint64_t typeid, const char* name){
  struct typeentry *te;
  if( sec_exists_entry(typeid) )
    return;
  te = malloc(sizeof(struct typeentry));
  te->id=typeid;
  strncpy(te->str, name, 256);
  HASH_ADD(hh, thash, id, sizeof(uint64_t), te);
}

bool type_find_entry(uint64_t typeid, char* name) {
  struct typeentry *te=NULL;
  HASH_FIND(hh, thash, &typeid, sizeof(uint64_t), te);
  if(!te)
    return false;
  strncpy(name, te->str, 256);
  return true;
}

static inline int provenance_type_id_to_str(uint64_t id,
                                char* name,
                                uint32_t len,
                                uint8_t is_relation){
  struct prov_type info;
  int rc;
  int fd;

  if( type_find_entry(id, name) )
    return 0;
  fd = open(PROV_TYPE, O_RDONLY);
  if( fd < 0 )
    return fd;
  memset(&info, 0, sizeof(struct prov_type));
  info.id=id;
  info.is_relation = is_relation;
  rc = read(fd, &info, sizeof(struct prov_type));
  close(fd);
  if(rc<0){
    name[0]='\0';
    return rc;
  }
  if(len<strlen(info.str))
    return -ENOMEM;
  strncpy(name, info.str, len);
  type_add_entry(id, name);
  return rc;
}

static __thread char name_buff[256];
char* relation_id_to_str(uint64_t id){
  int rc;
  provenance_type_id_to_str(id, name_buff, 256, 1);
  return name_buff;
}

char* node_id_to_str(uint64_t id){
  int rc;
  provenance_type_id_to_str(id, name_buff, 256, 0);
  return name_buff;
}

static inline int provenance_type_str_to_id(uint64_t *id,
                                const char* name,
                                uint32_t len,
                                uint8_t is_relation){
  struct prov_type info;
  int rc;
  int fd;

  fd = open(PROV_TYPE, O_RDONLY);
  if( fd < 0 )
    return fd;
  memset(&info, 0, sizeof(struct prov_type));
  strncpy(info.str, name, len);
  info.is_relation = is_relation;
  rc = read(fd, &info, sizeof(struct prov_type));
  close(fd);
  if(rc<0){
    *id = 0;
  }
  *id = info.id;
  return rc;
}

uint64_t relation_str_to_id(const char* name, uint32_t len){
  uint64_t id;
  provenance_type_str_to_id(&id, name, len, 1);
  return id;
}

uint64_t node_str_to_id(const char* name, uint32_t len){
  uint64_t id;
  provenance_type_str_to_id(&id, name, len, 0);
  return id;
}

#define declare_set_secctx_fcn(fcn_name, operation) int fcn_name (const char* secctx){\
  struct secinfo filter;\
  int rc;\
  int fd = open(PROV_SECCTX_FILTER, O_WRONLY);\
  if( fd < 0 ){\
    return fd;\
  }\
  strncpy(filter.secctx, secctx, PATH_MAX);\
  filter.len=strlen(filter.secctx);\
  filter.op = operation;\
  rc = write(fd, &filter, sizeof(struct secinfo));\
  close(fd);\
  return rc;\
}

declare_set_secctx_fcn(provenance_secctx_track, PROV_SET_TRACKED);
declare_set_secctx_fcn(provenance_secctx_propagate, PROV_SET_TRACKED|PROV_SET_PROPAGATE);
declare_set_secctx_fcn(provenance_secctx_delete, PROV_SET_DELETE);

int provenance_secctx( struct secinfo* filters, size_t length ){
  int rc;
  int fd = open(PROV_SECCTX_FILTER, O_RDONLY);
  if( fd < 0 ){
    return fd;
  }
  rc = read(fd, filters, length);
  close(fd);
  return rc;
}

#define declare_set_cgroup_fcn(fcn_name, operation) int fcn_name (const uint32_t cid){\
  struct nsinfo filter;\
  memset(&filter, 0, sizeof(struct nsinfo));\
  int rc;\
  int fd = open(PROV_NS_FILTER, O_WRONLY);\
  if( fd < 0 ){\
    return fd;\
  }\
  filter.cgroupns = cid;\
  filter.op = operation;\
  rc = write(fd, &filter, sizeof(struct nsinfo));\
  close(fd);\
  return rc;\
}

#define declare_get_ns_fcn(fcn_name) int fcn_name ( struct nsinfo* filters, size_t length ){\
  int rc;\
  int fd = open(PROV_NS_FILTER, O_RDONLY);\
  if( fd < 0 )\
    return fd;\
  rc = read(fd, filters, length);\
  close(fd);\
  return rc;\
}

declare_set_cgroup_fcn(provenance_cgroup_track, PROV_SET_TRACKED);
declare_set_cgroup_fcn(provenance_cgroup_propagate, PROV_SET_TRACKED|PROV_SET_PROPAGATE);
declare_set_cgroup_fcn(provenance_cgroup_delete, PROV_SET_DELETE);

declare_get_ns_fcn(provenance_ns);

int provenance_policy_hash(uint8_t* buffer, size_t length){
  int rc;
  int fd = open(PROV_POLICY_HASH_FILE, O_RDONLY);
  if(fd<0)
    return fd;
  rc = read(fd, buffer, length);
  close(fd);
  return rc;
}

#define declare_set_user_fcn(fcn_name, operation) int fcn_name (const char* uname){\
  struct userinfo filter;\
  struct passwd *pwd;\
  int rc;\
  int fd = open(PROV_UID_FILTER, O_WRONLY);\
  if( fd < 0 )\
    return fd;\
  pwd = getpwnam(uname);\
  if(!pwd)\
    return -EINVAL;\
  filter.uid=pwd->pw_uid;\
  filter.op = operation;\
  rc = write(fd, &filter, sizeof(struct userinfo));\
  close(fd);\
  return rc;\
}

declare_set_user_fcn(provenance_user_track, PROV_SET_TRACKED);
declare_set_user_fcn(provenance_user_propagate, PROV_SET_TRACKED|PROV_SET_PROPAGATE);
declare_set_user_fcn(provenance_user_delete, PROV_SET_DELETE);

int provenance_user(struct userinfo* filters, size_t length ){
  int rc;
  int fd = open(PROV_UID_FILTER, O_RDONLY);
  if( fd < 0 ){
    return fd;
  }
  rc = read(fd, filters, length);
  close(fd);
  return rc;
}


#define declare_set_group_fcn(fcn_name, operation) int fcn_name (const char* uname){\
  struct groupinfo filter;\
  struct group *gr;\
  int rc;\
  int fd = open(PROV_GID_FILTER, O_WRONLY);\
  if( fd < 0 )\
    return fd;\
  gr = getgrnam(uname);\
  if(!gr)\
    return -EINVAL;\
  filter.gid=gr->gr_gid;\
  filter.op = operation;\
  rc = write(fd, &filter, sizeof(struct groupinfo));\
  close(fd);\
  return rc;\
}

declare_set_group_fcn(provenance_group_track, PROV_SET_TRACKED);
declare_set_group_fcn(provenance_group_propagate, PROV_SET_TRACKED|PROV_SET_PROPAGATE);
declare_set_group_fcn(provenance_group_delete, PROV_SET_DELETE);

int provenance_group(struct groupinfo* filters, size_t length ){
  int rc;
  int fd = open(PROV_GID_FILTER, O_RDONLY);
  if( fd < 0 )
    return fd;
  rc = read(fd, filters, length);
  close(fd);
  return rc;
}

int provenance_version(char* version, size_t len){
  int rc;
  int fd = open(PROV_VERSION, O_RDONLY);
  if( fd < 0 )
    return fd;
  rc = read(fd, version, len);
  close(fd);
  return rc;
}

int provenance_create_channel(const char name[PATH_MAX]){
  int rc;
  char buffer[PATH_MAX];

  if(strlen(name) > PATH_MAX)
    return -ENOMEM;
  int fd = open(PROV_CHANNEL, O_WRONLY);
  if( fd < 0 )
    return fd;
  rc = write(fd, name, strlen(name)+1);
  close(fd);
  return rc;
}
