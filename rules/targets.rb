
# add functions to this class to allow them to be used as command-line targets
# if no command-line target is chosen, "default" is run.
class Targets
	class Target
		def initialize(name, preqs, &block)
			@name = name
			@preqs = preqs
			@block = block
		end
		
		def invoke
			#puts "preqs of '#{@name}'"
			@preqs.each do |p| p.invoke end
			#puts "block of '#{@name}'"
			@block.call
		end
	end
	
	@@targets = {}
	
	def Targets.reset(args)
		@@targets = {}
		@@args = args
	end
	
	def Targets.add(args, &block)
		case args
		when Hash
			fail "Too Many Task Names: #{args.keys.join(' ')}" if args.size > 1
			fail "No Task Name Given" if args.size < 1
			name = args.keys[0]
			preqs = args[name]
			preqs = [preqs] if (String===preqs) || (Regexp===preqs) || (Proc===preqs) || (Symbol===preqs)
			preqs = preqs.collect do |p| @@targets[p] end
		else
			name = args
			preqs = []
		end
		#puts "Target add '#{name}'"
		@@targets.store(name, Target.new(name, preqs, &block))
	end
	
	# parse ARGV
	def Targets.setup
		@@goals = []
		if(defined?(@@args) != nil)
			#puts "Got args from reset."
			args = @@args
		else
			#puts "ARGS not defined; going with ARGV."
			args = ARGV
		end
		#puts args.inspect
		args.each do |a| handle_arg(a) end
		if(@@goals.empty?) then
			@@goals = [:default]
		end
	end
	
	def Targets.handle_arg(a)
		i = a.index('=')
		if(i) then
			set_constant(a[0, i], a[i+1 .. a.length])
		else
			@@goals += [a.to_sym]
		end
	end
	
	def Targets.invoke
		setup
		@@goals.each { |t|
			if(@@targets[t] == nil) then
				error "Does not have target '#{t}'"
			end
			@@targets[t].invoke
		}
	end
end

def target(args, &block)
	Targets.add(args, &block)
end