# pg_scws
[![Lang](https://img.shields.io/badge/Language-C%2FC%2B%2B-green.svg)]()
[![BSD](https://img.shields.io/badge/License-BSD-green.svg)]()
[![Extension](https://img.shields.io/badge/Extension-PostgreSQL-green.svg)]()

Postgresql full-text search extension for chinese  
It implements by importing swcs.  


INSTALL
-------

1. **Downloads**

  ```
  git clone https://github.com/jaiminpan/pg_scws
  ```
2. **Compile**

  Make sure PostgreSQL is installed and command `pg_config` could be runnable.  
  ```
  cd pg_scws
  USE_PGXS=1 make && make install
  ```

HOW TO USE & EXAMPLE
-------

  ```
  scws=# create extension pg_scws;
  CREATE EXTENSION

  scws=#  select * from to_tsvector('scwscfg', '小明硕士毕业于中国科学院计算所，后在日本京都大学深造');
                                  to_tsvector                                
  ---------------------------------------------------------------------------
   '中国科学院计算所':4 '小明':1 '日本京都大学':5 '毕业':3 '深造':6 '硕士':2
  (1 row)

  scws=#  select * from to_tsvector('scwscfg', '李小福是创新办主任也是云计算方面的专家');
                                     to_tsvector                                   
  ---------------------------------------------------------------------------------
   '专家':10 '主任':5 '云':7 '创新':3 '办':4 '方面':9 '是':2,6 '李小福':1 '计算':8
  (1 row)

  ```

## USER DEFINED DICTIONARY


## NOTE
It should work with PostgreSQL > 9.x
Now only tested with PostgreSQL 9.4


## MISC

[scws project](https://github.com/hightman/scws) when you only need scws

[zhparser project](https://github.com/amutu/zhparser) when you only need a thin pg layer
(you need install scws separately before using zhparser)

## Author
Jaimin Pan: jaimin.pan@gmail.com
