echo ("--------- Hello World ---------------");

function Player::foo($this)
{
    echo("foo! myvalue=" SPC $this.myvalue);
}

function MyPlayer::bar($this)
{
    echo("bar!");
}


$player = new Player() {
    class = "MyPlayer";
    myvalue = 1;
};
//TOFIX $player.dump();
//TOFIX quit();

$player.foo();
$player.bar();

echo($player.getId());

