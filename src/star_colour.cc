#include <star_colour.h>

auto star_colour(std::string_view type) noexcept -> uint32_t
  {
  // the exotic ones first, because several of them begin with the letter of an ordinary class
  if(type.starts_with("TTS"))
    return 0xffb070u;
  if(type.starts_with("AeBe"))
    return 0xf0f0ffu;
  if(type == "H" or type.contains("BlackHole"))
    return 0x505050u;
  if(type == "N")
    return 0x9fd8ffu;
  if(type.starts_with('D'))
    return 0xe8f0ffu;
  if(type.starts_with('W'))
    return 0x8fa8ffu;
  if(type.starts_with('C') or type.starts_with('S') or type == "MS")
    return 0xd05030u;
  switch(type.empty() ? '?' : type.front())
    {
    case 'O': return 0x9bb0ffu;
    case 'B': return 0xaabfffu;
    case 'A': return 0xdbe4ffu;
    case 'F': return 0xfff6e0u;
    // the tints the game's own system map gives them - an M star is peach there, not red, and the red
    // is left to the brown dwarfs, darkening as they cool
    case 'G': return 0xfff0a0u;
    case 'K': return 0xffcf7au;
    case 'M': return 0xffa468u;
    case 'L': return 0xd83c6au;
    case 'T': return 0xa8306eu;
    case 'Y': return 0x74304cu;
    default:  return 0xccccccu;
    }
  }
