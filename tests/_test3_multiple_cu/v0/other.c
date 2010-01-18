

typedef struct Foo baz;

baz* fubar;
//baz fubar2;//this would be illagl b/c size of Foo isn't known
extern int value1;

int grumble()
{
  return value1;
}

