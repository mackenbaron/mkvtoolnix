# coding: utf-8

def format_string_for_po str
  return '"' + str.gsub(/"/, '\"') + '"' unless /\\n./.match(str)

  ([ '""' ] + str.split(/(?<=\\n)/).map { |part| '"' + part.gsub(/"/, '\"') + '"' }).join("\n")
end

def unformat_string_for_po str
  str.gsub(/^"|"$/, '').gsub(/\\"/, '"')
end

def read_po file_name
  items   = [ { comments: [] } ]
  msgtype = nil
  line_no = 0

  add_line = lambda do |type, to_add|
    items.last[type] ||= []
    items.last[type]  += to_add if to_add.is_a?(Array)
    items.last[type]  << to_add if to_add.is_a?(String)
  end

  IO.readlines(file_name).each do |line|
    line_no += 1
    line.chomp!

    if line.empty?
      items << {} unless items.last.keys.empty?
      msgtype = nil

    elsif items.size == 1
      add_line.call :comments, line

    elsif /^#:\s*(.+)/.match(line)
      add_line.call :sources, $1.split(/\s+/)

    elsif /^#,\s*(.+)/.match(line)
      add_line.call :flags, $1.split(/,\s*/)

    elsif /^#\./.match(line)
      add_line.call :instructions, line

    elsif /^#~/.match(line)
      add_line.call :obsolete, line

    elsif /^#\|/.match(line)
      add_line.call :other, line

    elsif /^#\s/.match(line)
      add_line.call :comments, line

    elsif /^ ( msgid(?:_plural)? | msgstr (?: \[ \d+ \])? ) \s* (.+)/x.match(line)
      type, string = $1, $2
      msgtype      = type.gsub(/\[.*/, '').to_sym

      items.last[msgtype] ||= []
      items.last[msgtype]  << unformat_string_for_po(string)

    elsif /^"/.match(line)
      fail "read_po: #{file_name}:#{line_no}: string entry without prior msgid/msgstr for »#{line}«" unless msgtype
      items.last[msgtype].last << unformat_string_for_po(line)

    else
      fail "read_po: #{file_name}:#{line_no}: unrecognized line type for »#{line}«"

    end
  end

  items.pop if items.last.keys.empty?

  return items
end

def write_po file_name, items
  File.open(file_name, "w") do |file|
    items.each do |item|
      if item[:obsolete]
        file.puts(item[:obsolete].join("\n"))
        file.puts
        next
      end

      if item[:comments]
        file.puts(item[:comments].join("\n"))
      end

      if item[:instructions]
        file.puts(item[:instructions].join("\n"))
      end

      if item[:sources]
        file.puts(item[:sources].map { |source| "#: #{source}" }.join("\n").gsub(/,$/, ''))
      end

      if item[:flags]
        file.puts("#, " + item[:flags].join(", "))
      end

      if item[:other]
        file.puts(item[:other].join("\n"))
      end

      if item[:msgid]
        file.puts("msgid " + format_string_for_po(item[:msgid].first))
      end

      if item[:msgid_plural]
        file.puts("msgid_plural " + format_string_for_po(item[:msgid_plural].first))
      end

      if item[:msgstr]
        idx = 0

        item[:msgstr].each do |msgstr|
          suffix  = item[:msgid_plural] ? "[#{idx}]" : ""
          idx    += 1
          file.puts("msgstr#{suffix} " + format_string_for_po(msgstr))
        end
      end

      file.puts
    end
  end
end

def normalize_po file
  puts "NORMALIZE-PO #{file}"
  write_po file, read_po(file)
end