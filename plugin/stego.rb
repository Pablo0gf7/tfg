# stego.rb - Plugin de Metasploit minimalista con logs de depuración
# Coge la línea que escribes tras "stego" y la ejecuta usando un binario externo.
# Uso:
#   msf6 > load /ruta/a/stego.rb
#   msf6 > stego_set_bin /ruta/a/tu/binario
#   msf6 > stego_debug on          # (opcional) activa logs detallados
#   msf6 > stego <args...>         # ejecuta: /ruta/a/tu/binario <args...>

require 'msf/core'
require 'shellwords'

module Msf
  class Plugin::Stego < Msf::Plugin
    def name; 'stego'; end
    def desc; 'Ejecuta un binario externo con los argumentos que escribas tras el comando.'; end

    def initialize(framework, opts)
      super
      add_console_dispatcher(StegoDispatcher)
      print_good('Plugin "stego" cargado. Usa: stego_set_bin <ruta> y luego: stego <args>. Debug con: stego_debug on')
    end

    def cleanup
      remove_console_dispatcher(StegoDispatcher)
    end
  end

  class StegoDispatcher
    include Msf::Ui::Console::CommandDispatcher

    DEFAULT_BIN = '/opt/stego/stego'

    def initialize(driver)
      super(driver)
      @bin   = (ENV['STEGO_BIN'] && !ENV['STEGO_BIN'].empty?) ? ENV['STEGO_BIN'] : DEFAULT_BIN
      @debug = true  # por petición, iniciamos con debug activado
      dputs("Inicializado. Binario=#{@bin.inspect} (ENV['STEGO_BIN']=#{ENV['STEGO_BIN'].inspect})")
    end

    def name
      'Stego'
    end

    def commands
      {
        'stego'          => 'Ejecuta el binario con los argumentos indicados: stego <args...>',
        'stego_bin'      => 'Muestra la ruta del binario actual',
        'stego_set_bin'  => 'Establece la ruta del binario: stego_set_bin </ruta/al/binario>',
        'stego_debug'    => 'Activa/Desactiva el modo debug: stego_debug on|off|toggle'
      }
    end

    # Log condicionado por @debug (o VERBOSE con vprint_status)
    def dputs(msg)
      if @debug
        print_status("[DEBUG] #{msg}")
      else
        vprint_status("[DEBUG] #{msg}")  
      end
    end

    def ensure_bin!
      dputs("Verificando binario: #{@bin}")
      unless @bin && File.exist?(@bin)
        print_error("No existe el binario: #{@bin}")
        print_status('Configura con: stego_set_bin /ruta/al/binario o export STEGO_BIN')
        return false
      end
      unless File.executable?(@bin)
        print_error("El binario no es ejecutable: #{@bin}")
        return false
      end
      dputs("OK: binario encontrado y ejecutable")
      true
    end

    # Construye un string bonito para mostrar el comando que se va a ejecutar
    # y redacta posibles secretos tras flags como -k/-k/-m
    def display_command(bin, args, redact_flags = ['-k','-k','-m'])
      pieces = [bin] + args.dup
      printable = pieces.dup
      printable.each_with_index do |val, idx|
        if redact_flags.include?(val) && printable[idx+1]
          printable[idx+1] = '***'
        end
      end
      Shellwords.join(printable)
    end

    # Ejecuta: <binario> <args...>
    def cmd_stego(*args)
      dputs("PWD: #{Dir.pwd}")
      dputs("ARGS (array): #{args.inspect}")
      return unless ensure_bin!
      cmd_line = display_command(@bin, args)
      print_status("Ejecutando: #{cmd_line}")
      # Usamos system(argv array) para que stdout/stderr del binario salgan en msfconsole
      ok = system(@bin, *args)
      code = $?.exitstatus rescue nil
      dputs("Exit status: #{code.inspect}")
      if ok
        print_good('Comando finalizado correctamente.')
      else
        print_error("Fallo ejecutando el comando#{code ? " (exit #{code})" : ''}.")
      end
    end

    def cmd_stego_bin(*_args)
      print_status("Binario: #{@bin}")
      print_status("Existe: #{File.exist?(@bin)}  Ejecutable: #{File.executable?(@bin)}")
    end

    def cmd_stego_set_bin(*args)
      if args.nil? || args.empty?
        print_error('Uso: stego_set_bin </ruta/al/binario>')
        return
      end
      candidate = args.join(' ').strip
      if candidate.empty?
        print_error('Ruta vacía.')
        return
      end
      @bin = candidate
      dputs("Seteado binario via comando: #{@bin}")
      print_good("Binario configurado: #{@bin}")
      cmd_stego_bin
    end

    def cmd_stego_debug(*args)
      arg = args.first&.downcase
      case arg
      when 'on','1','true'
        @debug = true
      when 'off','0','false'
        @debug = false
      when 'toggle'
        @debug = !@debug
      when nil
        # Mostrar estado si no se pasa argumento
      else
        print_error('Uso: stego_debug on|off|toggle')
        return
      end
      print_status("Debug: #{@debug ? 'ON' : 'OFF'}")
    end

    # Ayuda específica (opcional)
    def cmd_stego_help
      print_line('Uso:')
      print_line('  stego_set_bin </ruta/al/binario>  # establece el binario a usar')
      print_line('  stego <args...>                   # ejecuta el binario con los argumentos dados')
      print_line('  stego_bin                         # muestra la ruta actual del binario')
      print_line('  stego_debug on|off|toggle         # activa/desactiva logs de depuración')
    end
  end
end
