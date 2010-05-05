
%.beam: %.erl
	erlc -o test/ $<

all:
	./rebar compile

check: test/etap.beam
	prove test/*.t

clean:
	./rebar clean
	rm test/*.beam
