#include "bin/Wheeler/WheelItems/NamedPowerIcon.h"
#include <cstdlib>
#include <iostream>

static int checks = 0;
static void require(bool value, const char* message)
{
    ++checks;
    if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}

int main()
{
    using NamedPowerIcon::IsOmenOfGluttony;
    require(IsOmenOfGluttony("Omen of Gluttony", true), "Original power matches");
    require(IsOmenOfGluttony("OMEN OF GLUTTONY", true), "Uppercase display name matches");
    require(IsOmenOfGluttony("omen of gluttony", true), "Lowercase display name matches");
    require(!IsOmenOfGluttony("Omen of Gluttony", false), "An ordinary spell cannot borrow the power icon");
    require(!IsOmenOfGluttony("Greater Omen of Gluttony", true), "A prefixed power retains its icon");
    require(!IsOmenOfGluttony("Omen of Gluttony II", true), "A suffixed power retains its icon");
    require(!IsOmenOfGluttony("Omen of Greed", true), "Other Black Book powers retain their icons");
    require(!IsOmenOfGluttony("", true), "Missing name is safe");
    std::cout << checks << " named-power icon checks passed\n";
}
