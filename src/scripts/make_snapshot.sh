cp -r my_test /home/student/mnt/fuse/
curl http://localhost:8888/admin/stat 
./snapshot /home/student/mnt/fuse/.snapshot s
curl http://localhost:8888/admin/stat 
cp big4 /home/student/mnt/fuse/
curl http://localhost:8888/admin/stat 
./snapshot /home/student/mnt/fuse/.snapshot s
curl http://localhost:8888/admin/stat 
