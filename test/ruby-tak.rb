#!/usr/bin/env ruby

require 'uri'
require 'cgi'

def tak(x, y, z)
  if x <= y
    y
  else
    tak(
      tak(x-1,y,z),tak(y-1,z,x),tak(z-1,x,y))
  end
end

i = 0
10.times { p "Try:", i; i+=1; tak(12,6,0) }

