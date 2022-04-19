i=0
for clause in 'schedule(static)'  'schedule(static, 4)' 'schedule(static, 8)' 'schedule(dynamic)' 'schedule(dynamic, 4)' 'schedule(dynamic, 8)' 'schedule(dynamic) collapse(2)' 'schedule(guided)' 'schedule(guided, 4)' 'schedule(guided, 8)'; do
	i=$((i+1))

	echo $clause
	cp 3t.cpp 3_$i.cpp
	sed -i 's/___/'"$clause"'/g' 3_$i.cpp
	g++ --std=c++11 3_$i.cpp -lminirt -fopenmp -O2 -o 3_$i
	./3_$i

done
