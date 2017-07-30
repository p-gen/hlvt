bin_PROGRAMS = hlvt
hlvt_SOURCES = hlvt.c vtparse.c vtparse.h vtparse_table.c vtparse_table.h
#dist_man_MANS = hlvt.1
EXTRA_DIST = LICENSE.rst README.rst README.vtparse TODO.sequences \
             build-aux version vtparse_check_tables.rb            \
	     vtparse_gen_c_tables.rb vtparse_tables.rb            \
	     .clang-format

hlvt.c: vtparse_table.c vtparse_table.h

vtparse_table.c: vtparse_gen_c_tables.rb
	$(RUBY) vtparse_gen_c_tables.rb

vtparse_table.h: vtparse_gen_c_tables.rb
	$(RUBY) vtparse_gen_c_tables.rb

