# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF', <<'EOF']);
(read-stdin) begin
(read-stdin) end
read-stdin: exit(0)
EOF
(read-stdin) begin
read-stdin: exit(-1)
EOF
pass;
